module;
#include <SDL2/SDL.h>
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

export namespace my_scenes {

// ユーザーが実装する範囲 START

using scene_fw::Env;

// 各シーンの「純粋データ」
struct InitialData {
    std::shared_ptr<const global_setting::GlobalSetting> setting;
    std::unique_ptr<grid::Grid> grid;
    std::unique_ptr<tetrimino::Tetrimino> tetrimino;

    InitialData() = default;

    InitialData(const InitialData& other)
        : setting(other.setting),
          grid(other.grid ? std::make_unique<grid::Grid>(*other.grid) : nullptr),
          tetrimino(other.tetrimino ? std::make_unique<tetrimino::Tetrimino>(*other.tetrimino)
                                    : nullptr) {}

    InitialData& operator=(const InitialData& other) {
        if (this == &other) return *this;
        setting = other.setting;
        grid = other.grid ? std::make_unique<grid::Grid>(*other.grid) : nullptr;
        tetrimino =
            other.tetrimino ? std::make_unique<tetrimino::Tetrimino>(*other.tetrimino) : nullptr;
        return *this;
    }

    InitialData(InitialData&&) noexcept = default;
    InitialData& operator=(InitialData&&) noexcept = default;
};

struct NextData {};
struct ThirdData {};

// すべてのシーンを包含する代数的データ型
using Scene = std::variant<InitialData, NextData, ThirdData>;

// 初期シーンの生成
inline tl::expected<Scene, std::string> make_initial(
    std::shared_ptr<const global_setting::GlobalSetting> gs) {
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

    return Scene{s};
}

// --- Initial シーン: update / render ---
inline Scene update(const InitialData& s, const Env& env) {
    if (env.input.pressed(input::InputKey::PAUSE)) {
        NextData next{};
        return Scene{next};  // 遷移
    }
    InitialData updated = s;
    // ... 更新処理 ...
    return Scene{updated};  // 継続
}

inline void render(const InitialData& s, SDL_Renderer* renderer) {
    const auto& setting = *s.setting;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    grid::render(*s.grid, renderer);
    tetrimino::render(*s.tetrimino, setting.cellWidth, setting.cellHeight, renderer);
    tetrimino::render_grid_around(*s.tetrimino, renderer, setting.cellWidth, setting.cellHeight);
    SDL_RenderPresent(renderer);
}

// --- Next シーン ---
inline Scene update(const NextData& s, const Env& env) {
    if (env.input.pressed(input::InputKey::PAUSE)) {
        ThirdData third{};
        return Scene{third};
    }
    return Scene{s};
}

inline void render(const NextData&, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

// --- Third シーン ---
inline Scene update(const ThirdData& s, const Env& env) {
    if (env.input.pressed(input::InputKey::PAUSE)) {
        auto initial = make_initial(std::make_shared<global_setting::GlobalSetting>(env.setting));
        if (!initial) {
            return Scene{s};
        }
        return initial.value();
    }
    return Scene{s};
}

inline void render(const ThirdData&, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

// フレームワークに渡すエントリ
struct Impl {
    using Scene = my_scenes::Scene;

    static tl::expected<Scene, std::string> make_initial(
        std::shared_ptr<const global_setting::GlobalSetting> gs) {
        return my_scenes::make_initial(std::move(gs));
    }

    static Scene step(Scene current, const Env& env) {
        return std::visit([&](auto const& ss) -> Scene { return update(ss, env); }, current);
    }

    static void draw(const Scene& current, SDL_Renderer* r) {
        std::visit([&](auto const& ss) { render(ss, r); }, current);
    }
};

// ユーザーが実装する範囲 END

}  // namespace my_scenes
