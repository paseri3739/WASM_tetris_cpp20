module;
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <array>
export module TetriMino;
import Position2D;
import Cell;
import GlobalSetting;
namespace tetrimino {
export enum class TetriminoType { I, O, T, S, Z, J, L };
export enum class TetriminoStatus { Falling, Landed, Merged };
export enum class TetriminoDirection { North, East, South, West };
constexpr SDL_Colour to_color(TetriminoType type) noexcept {
    switch (type) {
        case TetriminoType::I:
            return SDL_Colour{0, 255, 255, 255};  // シアン
        case TetriminoType::O:
            return SDL_Colour{255, 255, 0, 255};  // 黄色
        case TetriminoType::T:
            return SDL_Colour{128, 0, 128, 255};  // 紫
        case TetriminoType::S:
            return SDL_Colour{0, 255, 0, 255};  // 緑
        case TetriminoType::Z:
            return SDL_Colour{255, 0, 0, 255};  // 赤
        case TetriminoType::J:
            return SDL_Colour{0, 0, 255, 255};  // 青
        case TetriminoType::L:
            return SDL_Colour{255, 165, 0, 255};  // オレンジ
    }
    return SDL_Colour{255, 255, 255, 255};  // 白（デフォルト）
};

// 座標は (row, col)。左上が (0,0)。
using Coord = std::pair<std::int8_t, std::int8_t>;

// N(北): 与えられた 4x4 テーブルと同一配置の相対座標
constexpr std::array<Coord, 4> get_cells_north(TetriminoType type) noexcept {
    switch (type) {
        case TetriminoType::I:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{1, 3}};
        case TetriminoType::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case TetriminoType::T:
            return {Coord{0, 1}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
        case TetriminoType::S:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 0}, Coord{1, 1}};
        case TetriminoType::Z:
            return {Coord{0, 0}, Coord{0, 1}, Coord{1, 1}, Coord{1, 2}};
        case TetriminoType::J:
            return {Coord{0, 0}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
        case TetriminoType::L:
            return {Coord{0, 2}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
    }
    return {};  // 到達しない
}

// E(東): 4x4 グリッド内で N を時計回り 90°回転（(r,c)->(c, 3-r)）した相対座標
constexpr std::array<Coord, 4> get_cells_east(TetriminoType type) noexcept {
    switch (type) {
        case TetriminoType::I:
            return {Coord{0, 2}, Coord{1, 2}, Coord{2, 2}, Coord{3, 2}};
        case TetriminoType::O:
            return {Coord{1, 2}, Coord{1, 3}, Coord{2, 2}, Coord{2, 3}};
        case TetriminoType::T:
            return {Coord{0, 2}, Coord{1, 2}, Coord{1, 3}, Coord{2, 2}};
        case TetriminoType::S:
            return {Coord{0, 2}, Coord{1, 2}, Coord{1, 3}, Coord{2, 3}};
        case TetriminoType::Z:
            return {Coord{0, 3}, Coord{1, 2}, Coord{1, 3}, Coord{2, 2}};
        case TetriminoType::J:
            return {Coord{0, 2}, Coord{0, 3}, Coord{1, 2}, Coord{2, 2}};
        case TetriminoType::L:
            return {Coord{0, 2}, Coord{1, 2}, Coord{2, 2}, Coord{2, 3}};
    }
    return {};
}

// S(南): 4x4 グリッド内で N を 180°回転（(r,c)->(3-r, 3-c)）
constexpr std::array<Coord, 4> get_cells_south(TetriminoType type) noexcept {
    switch (type) {
        case TetriminoType::I:
            return {Coord{2, 0}, Coord{2, 1}, Coord{2, 2}, Coord{2, 3}};
        case TetriminoType::O:
            return {Coord{2, 1}, Coord{2, 2}, Coord{3, 1}, Coord{3, 2}};
        case TetriminoType::T:
            return {Coord{2, 1}, Coord{2, 2}, Coord{2, 3}, Coord{3, 2}};
        case TetriminoType::S:
            return {Coord{2, 2}, Coord{2, 3}, Coord{3, 1}, Coord{3, 2}};
        case TetriminoType::Z:
            return {Coord{2, 1}, Coord{2, 2}, Coord{3, 2}, Coord{3, 3}};
        case TetriminoType::J:
            return {Coord{2, 1}, Coord{2, 2}, Coord{2, 3}, Coord{3, 3}};
        case TetriminoType::L:
            return {Coord{2, 1}, Coord{2, 2}, Coord{2, 3}, Coord{3, 1}};
    }
    return {};
}

// W(西): 4x4 グリッド内で N を反時計回り 90°（(r,c)->(3-c, r)）
constexpr std::array<Coord, 4> get_cells_west(TetriminoType type) noexcept {
    switch (type) {
        case TetriminoType::I:
            return {Coord{0, 1}, Coord{1, 1}, Coord{2, 1}, Coord{3, 1}};
        case TetriminoType::O:
            return {Coord{1, 0}, Coord{1, 1}, Coord{2, 0}, Coord{2, 1}};
        case TetriminoType::T:
            return {Coord{1, 1}, Coord{2, 0}, Coord{2, 1}, Coord{3, 1}};
        case TetriminoType::S:
            return {Coord{1, 0}, Coord{2, 0}, Coord{2, 1}, Coord{3, 1}};
        case TetriminoType::Z:
            return {Coord{1, 1}, Coord{2, 0}, Coord{2, 1}, Coord{3, 0}};
        case TetriminoType::J:
            return {Coord{1, 1}, Coord{2, 1}, Coord{3, 0}, Coord{3, 1}};
        case TetriminoType::L:
            return {Coord{1, 0}, Coord{1, 1}, Coord{2, 1}, Coord{3, 1}};
    }
    return {};
}

export struct Tetrimino {
    const TetriminoType type;
    const TetriminoStatus status;
    const TetriminoDirection direction;
    const Position2D position;
};

export void render(const Tetrimino& tetrimino, SDL_Renderer* renderer) {
    const SDL_Colour color = to_color(tetrimino.type);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    std::array<Coord, 4> cells;
    switch (tetrimino.status) {
        case TetriminoStatus::Falling:
        case TetriminoStatus::Landed:
        case TetriminoStatus::Merged:
            switch (tetrimino.direction) {
                case TetriminoDirection::North:
                    cells = get_cells_north(tetrimino.type);
                    break;
                case TetriminoDirection::East:
                    cells = get_cells_east(tetrimino.type);
                    break;
                case TetriminoDirection::South:
                    cells = get_cells_south(tetrimino.type);
                    break;
                case TetriminoDirection::West:
                    cells = get_cells_west(tetrimino.type);
                    break;
            }
            break;
    }
    const auto setting = global_setting::GlobalSetting::instance();
    const int cell_width = setting.cellWidth;
    const int cell_height = setting.cellHeight;

    for (const auto& cell : cells) {
        const int x = tetrimino.position.x + cell.second * cell_width;
        const int y = tetrimino.position.y + cell.first * cell_height;
        SDL_Rect rect = {x, y, cell_width, cell_height};
        SDL_RenderFillRect(renderer, &rect);
    }
};

// 4x4 の外枠を含めた格子を描画する。
// origin は格子の左上ピクセル位置。
// 枠線色は白、線幅は SDL のデフォルト（1px）。
export void render_grid_4x4(const Position2D& origin, SDL_Renderer* renderer) {
    const auto setting = global_setting::GlobalSetting::instance();
    const int cell_width = setting.cellWidth;
    const int cell_height = setting.cellHeight;

    const int grid_width = cell_width * 4;
    const int grid_height = cell_height * 4;

    // 枠線色（白）
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // 外枠
    SDL_Rect frame = {origin.x, origin.y, grid_width, grid_height};
    SDL_RenderDrawRect(renderer, &frame);

    // 内側の縦線（1～3 列目の境界）
    for (int c = 1; c <= 3; ++c) {
        const int x = origin.x + c * cell_width;
        SDL_RenderDrawLine(renderer, x, origin.y, x, origin.y + grid_height);
    }

    // 内側の横線（1～3 行目の境界）
    for (int r = 1; r <= 3; ++r) {
        const int y = origin.y + r * cell_height;
        SDL_RenderDrawLine(renderer, origin.x, y, origin.x + grid_width, y);
    }
}

// Tetrimino の 4x4 ローカルグリッド位置に格子を重ねて描画するラッパー。
export void render_grid_around(const Tetrimino& tetrimino, SDL_Renderer* renderer) {
    render_grid_4x4(tetrimino.position, renderer);
}

}  // namespace tetrimino
