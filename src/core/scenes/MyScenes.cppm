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

inline Scene update(const InitialData& s, const Env<global_setting::GlobalSetting>& env) {
    // 入力に応じて遷移
    const auto key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (env.input.pressed(*key)) {
        NextData next{};
        return Scene{next};  // 遷移
    }

    // シーン状態をコピー（Grid はディープコピー, registry は共有）
    InitialData updated = s;

    if (updated.registry && updated.grid) {
        auto& registry = *updated.registry;
        auto& grid = *updated.grid;

        // 経過時間を加算
        updated.fall_accumulator += env.dt;

        // 一定間隔ごとに1マス落下させる
        const double fall_interval = env.setting.dropRate;  // 秒: 適宜調整
        while (updated.fall_accumulator >= fall_interval) {
            updated.fall_accumulator -= fall_interval;

            auto view = registry.view<Position, TetriminoMeta, TetriminoTag>();
            for (auto entity : view) {
                auto& pos = view.get<Position>(entity);
                auto& meta = view.get<TetriminoMeta>(entity);

                if (meta.status != tetrimino::TetriminoStatus::Falling) {
                    continue;
                }

                // 現在の ECS 状態から Tetrimino を組み立ててルールに渡す
                tetrimino::Tetrimino current{meta.type, meta.status, meta.direction,
                                             Position2D{pos.x, pos.y}};

                auto res = tetris_rule::drop(current, grid, env);

                if (res) {
                    const auto& next = res.value();
                    // 成功時: 位置・状態を反映
                    pos.x = next.position.x;
                    pos.y = next.position.y;
                    meta.status = next.status;
                    meta.direction = next.direction;
                } else {
                    // 失敗時: 衝突 or 範囲外とみなし着地扱い
                    switch (res.error()) {
                        case tetris_rule::FailReason::OutOfBounds:
                        case tetris_rule::FailReason::Collision:
                            meta.status = tetrimino::TetriminoStatus::Landed;
                            break;
                    }
                }
            }
        }
    }

    return Scene{std::move(updated)};  // 継続
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
