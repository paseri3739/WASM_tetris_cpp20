module;
#include <SDL2/SDL.h>
#include <iostream>
#include <memory>
export module Scene;
import GlobalSetting;
import Input;
import Grid;
import Cell;
import TetriMino;
import Position2D;
// =============================
// シーン(関数型)定義:variant ベース
// =============================
export namespace scene {

// フレームワークSTART
// フレーム毎に渡す不変の環境
struct Env {
    const input::Input& input;
    const global_setting::GlobalSetting& setting;
    double dt;
};
// フレームワークEND

// ユーザーが実装する範囲 START
// 各シーンの「純粋データ」
struct InitialData {
    std::shared_ptr<const global_setting::GlobalSetting> setting;
    std::unique_ptr<grid::Grid> grid;
    std::unique_ptr<tetrimino::Tetrimino> tetrimino;

    // 追加: デフォルト
    InitialData() = default;

    // 追加: ディープコピー用コピーコンストラクタ
    InitialData(const InitialData& other)
        : setting(other.setting),
          grid(other.grid ? std::make_unique<grid::Grid>(*other.grid) : nullptr),
          tetrimino(other.tetrimino ? std::make_unique<tetrimino::Tetrimino>(*other.tetrimino)
                                    : nullptr) {}

    // 追加: ディープコピー用コピー代入
    InitialData& operator=(const InitialData& other) {
        if (this == &other) return *this;
        setting = other.setting;
        grid = other.grid ? std::make_unique<grid::Grid>(*other.grid) : nullptr;
        tetrimino =
            other.tetrimino ? std::make_unique<tetrimino::Tetrimino>(*other.tetrimino) : nullptr;
        return *this;
    }

    // 追加: ムーブは従来通り
    InitialData(InitialData&&) noexcept = default;
    InitialData& operator=(InitialData&&) noexcept = default;
};

struct NextData {
    // 必要であればデータを追加
};

// 追加: 三つ目のシーン
struct ThirdData {
    // 必要であればデータを追加
};

// すべてのシーンを包含する代数的データ型
using Scene = std::variant<InitialData, NextData, ThirdData>;

// 初期シーンの生成(旧実装のロジックを移植)
inline InitialData make_initial(std::shared_ptr<const global_setting::GlobalSetting> gs) {
    InitialData s{};
    s.setting = std::move(gs);

    const auto grid_res = grid::Grid::create(
        "initial_scene_grid", Position2D{0, 0}, s.setting->canvasWidth, s.setting->canvasHeight,
        s.setting->gridRows, s.setting->gridColumns, s.setting->cellWidth, s.setting->cellHeight);

    if (!grid_res) {
        std::cerr << "Failed to create grid: " << grid_res.error() << std::endl;
        const auto grid =
            grid::Grid::create("fallback", Position2D{0, 0}, s.setting->canvasWidth,
                               s.setting->canvasHeight, s.setting->gridRows, s.setting->gridColumns,
                               s.setting->cellWidth, s.setting->cellHeight);
        // フォールバック(適宜調整)
        s.grid = std::make_unique<grid::Grid>(grid.value());
    } else {
        s.grid = std::make_unique<grid::Grid>(grid_res.value());
    }

    const auto cell_pos = grid::get_cell_position(*s.grid, 3, 3);
    if (!cell_pos) {
        std::cerr << "Failed to get cell position: " << cell_pos.error() << std::endl;
    }

    auto t = tetrimino::Tetrimino(tetrimino::TetriminoType::Z, tetrimino::TetriminoStatus::Falling,
                                  tetrimino::TetriminoDirection::West,
                                  cell_pos ? cell_pos.value() : Position2D{0, 0});
    s.tetrimino = std::make_unique<tetrimino::Tetrimino>(std::move(t));

    return s;
}

// --- Initial シーン: update / render ---
inline Scene update(const InitialData& s, const Env& env) {
    // 入力に応じて遷移
    if (env.input.pressed(input::InputKey::PAUSE)) {
        NextData next{};
        return Scene{next};  // 遷移
    }

    // 例:物理・落下などの更新があればここで s を複製して書き換え
    // ディープコピー
    InitialData updated = s;
    // ... 更新処理 ...

    return Scene{updated};  // 継続
}

inline void render(const InitialData& s, SDL_Renderer* renderer) {
    const auto& setting = *s.setting;
    // 背景を白にクリア
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    grid::render(*s.grid, renderer);
    tetrimino::render(*s.tetrimino, setting.cellWidth, setting.cellHeight, renderer);
    tetrimino::render_grid_around(*s.tetrimino, renderer, setting.cellWidth, setting.cellHeight);
    // 描画内容を画面に反映
    SDL_RenderPresent(renderer);
}

// --- Next シーン: update / render ---
inline Scene update(const NextData& s, const Env& env) {
    // 入力や時間経過に応じた次画面への遷移があればここで返す
    // 追加: PAUSE が押されたら ThirdData へ遷移
    if (env.input.pressed(input::InputKey::PAUSE)) {
        ThirdData third{};
        return Scene{third};
    }
    return Scene{s};  // 継続(遷移なし)
}

inline void render(const NextData& /*s*/, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

// --- Third シーン: update / render ---
inline Scene update(const ThirdData& s, const Env& env) {
    // 入力や時間経過に応じた次画面への遷移があればここで返す

    // 追加: PAUSE が押されたら InitialData に戻る
    if (env.input.pressed(input::InputKey::PAUSE)) {
        // setting は他シーンと同様に env から渡す
        InitialData initial =
            make_initial(std::make_shared<global_setting::GlobalSetting>(env.setting));
        return Scene{initial};
    }

    return Scene{s};  // 継続(遷移なし)
}

inline void render(const ThirdData& /*s*/, SDL_Renderer* renderer) {
    // 追加: 背景を赤色でクリア
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}
// ユーザーが実装する範囲 END

// フレームワーク側 START
// ディスパッチ:1ステップ更新 / 描画
inline Scene step(Scene current, const Env& env) {
    return std::visit([&](auto const& ss) -> Scene { return update(ss, env); }, current);
}

inline void draw(const Scene& current, SDL_Renderer* r) {
    std::visit([&](auto const& ss) { render(ss, r); }, current);
}
// フレームワーク側 END

}  // namespace scene
