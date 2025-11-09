module;
#include <SDL2/SDL.h>
#include <entt/entt.hpp>
#include <iostream>
#include <memory>
#include <tl/expected.hpp>

export module MyScenes;  // 旧 Scene ではなくユーザー側

import SceneFramework;
import GlobalSetting;
import Input;
import Grid;
import Cell;
import TetriMino;
import Position2D;
import TetrisRule; // ★ 追加
import GameKey;

export namespace my_scenes {

// ユーザーが実装する範囲 START

using scene_fw::Env;

// =============================
// ECS 用コンポーネント定義
// =============================

// 位置コンポーネント
struct Position {
    int x;
    int y;
};

// テトリミノ状態コンポーネント
struct TetriminoMeta {
    tetrimino::TetriminoType type;
    tetrimino::TetriminoDirection direction;
    tetrimino::TetriminoStatus status;
};

// Initial シーンで操作対象となるテトリミノ識別用タグ
struct TetriminoTag {};

// =============================
// 各シーンの「純粋データ」
// =============================

struct InitialData {
    std::shared_ptr<const global_setting::GlobalSetting> setting;
    std::unique_ptr<grid::Grid> grid;
    std::shared_ptr<entt::registry> registry;  // ECS world（共有ポインタでコピー可能）
    entt::entity active_tetrimino{entt::null};

    // ★ 一定時間ごとに1マス落下させるための蓄積タイマー
    double fall_accumulator = 0.0;
    double input_accumulator = 0.0;  // 連続入力のための累積時間

    InitialData() = default;

    // ディープコピー用コピーコンストラクタ
    InitialData(const InitialData& other)
        : setting(other.setting),
          grid(other.grid ? std::make_unique<grid::Grid>(*other.grid) : nullptr),
          // registry は共有（必要なら後でクローン戦略を検討）
          registry(other.registry),
          active_tetrimino(other.active_tetrimino),
          fall_accumulator(other.fall_accumulator) {}

    // ディープコピー用コピー代入
    InitialData& operator=(const InitialData& other) {
        if (this == &other) return *this;
        setting = other.setting;
        grid = other.grid ? std::make_unique<grid::Grid>(*other.grid) : nullptr;
        registry = other.registry;
        active_tetrimino = other.active_tetrimino;
        fall_accumulator = other.fall_accumulator;
        return *this;
    }

    // ムーブは従来通り
    InitialData(InitialData&&) noexcept = default;
    InitialData& operator=(InitialData&&) noexcept = default;
};

struct NextData {
    // 必要であればデータを追加
};

struct ThirdData {
    // 必要であればデータを追加
};

// すべてのシーンを包含する代数的データ型
using Scene = std::variant<InitialData, NextData, ThirdData>;

// =============================
// 初期シーンの生成 (ECS 導入版)
// =============================
inline tl::expected<Scene, std::string> make_initial(
    std::shared_ptr<const global_setting::GlobalSetting> gs) {
    InitialData s{};
    s.setting = std::move(gs);

    const auto grid_res = grid::Grid::create(
        "initial_scene_grid", Position2D{0, 0}, s.setting->canvasWidth, s.setting->canvasHeight,
        s.setting->gridRows, s.setting->gridColumns, s.setting->cellWidth, s.setting->cellHeight);

    if (!grid_res) {
        std::cerr << "Failed to create grid: " << grid_res.error() << std::endl;
        const auto fallback =
            grid::Grid::create("fallback", Position2D{0, 0}, s.setting->canvasWidth,
                               s.setting->canvasHeight, s.setting->gridRows, s.setting->gridColumns,
                               s.setting->cellWidth, s.setting->cellHeight);
        if (!fallback) {
            std::cerr << "Fallback grid creation failed: " << fallback.error() << std::endl;
            return tl::make_unexpected("Failed to create both initial and fallback grid: " +
                                       fallback.error());
        }
        s.grid = std::make_unique<grid::Grid>(fallback.value());
    } else {
        s.grid = std::make_unique<grid::Grid>(grid_res.value());
    }

    // EnTT レジストリ生成
    s.registry = std::make_shared<entt::registry>();

    // スポーン位置
    const auto cell_pos = grid::get_cell_position(*s.grid, 3, 3);
    if (!cell_pos) {
        std::cerr << "Failed to get cell position: " << cell_pos.error() << std::endl;
    }
    const Position2D spawn_pos = cell_pos ? cell_pos.value() : Position2D{0, 0};

    // テトリミノ用エンティティ
    const entt::entity e = s.registry->create();
    s.registry->emplace<Position>(e, spawn_pos.x, spawn_pos.y);
    s.registry->emplace<TetriminoMeta>(e, tetrimino::TetriminoType::Z,
                                       tetrimino::TetriminoDirection::West,
                                       tetrimino::TetriminoStatus::Falling);
    s.registry->emplace<TetriminoTag>(e);
    s.active_tetrimino = e;

    s.fall_accumulator = 0.0;

    return Scene{s};
}

// =============================
// --- Initial シーン: update / render (ECS + TetrisRule::drop)
// =============================
// 毎フレームの処理
// 毎フレームの処理（モナド連鎖で drop 処理を共通化）

// 毎フレームの処理（自然落下の加速方式 + モナド連鎖で分岐集約）

inline Scene update(const InitialData& s, const Env<global_setting::GlobalSetting>& env) {
    // 入力に応じて遷移
    const auto pause_key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (pause_key && env.input.pressed(*pause_key)) {
        NextData next{};
        return Scene{next};  // 遷移
    }

    InitialData updated = s;

    if (updated.registry && updated.grid) {
        auto& registry = *updated.registry;
        auto& grid = *updated.grid;

        // 時間を加算
        // 旧: updated.fall_accumulator は「秒」ベースのアキュムレータ
        // 新: 「セル数」ベースのアキュムレータとして運用（cells accumulator）
        //     → env.dt * rate(cells/sec) を足していき、floor 分だけ 1 セル落下
        //
        // 旧: updated.input_accumulator は使用しない（自然落下の倍率化に一本化）
        //     ※ 互換のため値は維持するが、ここではリセットのみ行う
        updated.input_accumulator = 0.0;  // 押しっぱなしの離散リピートは廃止

        // 落下レート計算（cells/sec）
        // 基本レート: 1 / dropRate（dropRate は「自然落下の間隔[秒/セル]」）
        const double base_rate = (env.setting.dropRate > 0.0) ? (1.0 / env.setting.dropRate) : 0.0;

        // ソフトドロップ倍率（押下中に加速）
        // ※ 設定に昇格していない場合のデフォルト値
        constexpr double kSoftMultiplier = 10.0;

        // 1フレーム当たりの最大落下回数（低FPS時の暴走抑止）
        constexpr int kMaxDropsPerFrame = 6;

        // 入力状態
        const auto down_key = game_key::to_sdl_key(game_key::GameKey::DOWN);
        const bool isHeldDown = (down_key && env.input.held(*down_key));

        // 現フレームの実効レート
        const double rate = base_rate * (isHeldDown ? kSoftMultiplier : 1.0);

        // セル数アキュムレータへ加算（cells）
        updated.fall_accumulator += env.dt * rate;

        // 取りこぼし防止のため整数セル分だけまとめて処理（過剰進行防止に上限を設ける）
        int steps_to_drop = static_cast<int>(std::floor(updated.fall_accumulator));
        if (steps_to_drop > kMaxDropsPerFrame) {
            steps_to_drop = kMaxDropsPerFrame;
        }
        updated.fall_accumulator -= static_cast<double>(steps_to_drop);

        // -------------------------------
        // 以降：モナド連鎖で drop 分岐を集約
        // -------------------------------

        // 1セル落下の“結果”を Tetrimino 本体ではなく構成要素で返す
        struct StepComponents {
            int x;
            int y;
            tetrimino::TetriminoStatus status;
            tetrimino::TetriminoDirection dir;
        };

        // 不変 Tetrimino を“構成要素”から都度生成するヘルパ
        auto make_tetr = [](tetrimino::TetriminoType type, tetrimino::TetriminoStatus st,
                            tetrimino::TetriminoDirection dir, int x, int y) {
            return tetrimino::Tetrimino{type, st, dir, Position2D{x, y}};
        };

        // 1セル落下（モナディック）：成功→次の構成要素、衝突/範囲外→Landed
        // に正規化、未実装→エラー伝播
        auto drop_onceM = [&](tetrimino::TetriminoType type, const StepComponents& cur)
            -> tl::expected<StepComponents, tetris_rule::FailReason> {
            auto cur_tetr = make_tetr(type, cur.status, cur.dir, cur.x, cur.y);

            return tetris_rule::drop(cur_tetr, grid, env)
                .transform([](const tetrimino::Tetrimino& t) -> StepComponents {
                    return StepComponents{t.position.x, t.position.y, t.status, t.direction};
                })
                .or_else([&](tetris_rule::FailReason e)
                             -> tl::expected<StepComponents, tetris_rule::FailReason> {
                    switch (e) {
                        case tetris_rule::FailReason::OutOfBounds:
                        case tetris_rule::FailReason::Collision:
                            // 破壊的代入は不可なので、新しい“構成要素”として Landed を返す
                            return StepComponents{cur.x, cur.y, tetrimino::TetriminoStatus::Landed,
                                                  cur.dir};
                        case tetris_rule::FailReason::NOT_IMPLEMENTED:
                        default:
                            return tl::unexpected{e};
                    }
                });
        };

        // ---- エンティティの更新（代入不可問題を回避しつつ分岐を集約）----
        auto view = registry.view<Position, TetriminoMeta, TetriminoTag>();
        for (auto entity : view) {
            auto& pos = view.get<Position>(entity);
            auto& meta = view.get<TetriminoMeta>(entity);

            if (meta.status != tetrimino::TetriminoStatus::Falling) {
                continue;
            }

            StepComponents out{pos.x, pos.y, meta.status, meta.direction};

            // steps_to_drop 回だけモナディックに合成（早期停止を含む）
            for (int i = 0; i < steps_to_drop; ++i) {
                if (out.status != tetrimino::TetriminoStatus::Falling) {
                    break;  // すでに着地なら終了
                }
                auto nxt = drop_onceM(meta.type, out);  // 新オブジェクトを“返す”
                if (!nxt) {
                    // NOT_IMPLEMENTED 等は現状維持で黙殺（必要ならログ）
                    // SDL_Log("drop NOT_IMPLEMENTED (entity=%d)", (int)entity);
                    break;
                }
                out = *nxt;  // StepComponents は代入可能（const メンバなし）
                if (out.status != tetrimino::TetriminoStatus::Falling) {
                    break;
                }
            }

            // 最終状態を反映（Tetrimino 本体に触れずスカラーだけ更新）
            pos.x = out.x;
            pos.y = out.y;
            meta.status = out.status;
            meta.direction = out.dir;
        }
    }

    return Scene{std::move(updated)};
}

inline void render(const InitialData& s, SDL_Renderer* renderer) {
    const auto& setting = *s.setting;

    // 背景を白にクリア
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    // Grid 描画
    if (s.grid) {
        grid::render(*s.grid, renderer);
    }

    // ECS からテトリミノ描画
    if (s.registry) {
        auto& registry = *s.registry;
        auto view = registry.view<const Position, const TetriminoMeta, const TetriminoTag>();

        for (auto entity : view) {
            const auto& pos = view.get<const Position>(entity);
            const auto& meta = view.get<const TetriminoMeta>(entity);

            tetrimino::Tetrimino t{meta.type, meta.status, meta.direction,
                                   Position2D{pos.x, pos.y}};

            tetrimino::render(t, setting.cellWidth, setting.cellHeight, renderer);
            tetrimino::render_grid_around(t, renderer, setting.cellWidth, setting.cellHeight);
        }
    }

    SDL_RenderPresent(renderer);
}

// =============================
// --- Next シーン: update / render ---
// =============================
inline Scene update(const NextData& s, const Env<global_setting::GlobalSetting>& env) {
    const auto key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (env.input.pressed(*key)) {
        ThirdData third{};
        return Scene{third};
    }
    return Scene{s};  // 継続
}

inline void render(const NextData&, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

// =============================
// --- Third シーン: update / render ---
// =============================
inline Scene update(const ThirdData& s, const Env<global_setting::GlobalSetting>& env) {
    const auto key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (env.input.pressed(*key)) {
        auto initial = make_initial(std::make_shared<global_setting::GlobalSetting>(env.setting));
        if (!initial) {
            return Scene{s};
        }
        return initial.value();
    }
    return Scene{s};  // 継続
}

inline void render(const ThirdData&, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

// =============================
// フレームワークに渡すエントリ
// =============================
struct Impl {
    using Scene = my_scenes::Scene;

    static tl::expected<Scene, std::string> make_initial(
        std::shared_ptr<const global_setting::GlobalSetting> gs) {
        return my_scenes::make_initial(std::move(gs));
    }

    static Scene step(Scene current, const Env<global_setting::GlobalSetting>& env) {
        return std::visit([&](auto const& ss) -> Scene { return update(ss, env); }, current);
    }

    static void draw(const Scene& current, SDL_Renderer* r) {
        std::visit([&](auto const& ss) { render(ss, r); }, current);
    }
};

// ユーザーが実装する範囲 END

}  // namespace my_scenes
