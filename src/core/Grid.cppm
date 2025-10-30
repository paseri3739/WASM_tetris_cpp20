module;
#include <SDL2/SDL.h>
#include <string>
#include <vector>
export module Grid;
import Position2D;
import Cell;

namespace grid {
export struct Grid {
    const std::string id;
    const Position2D position;
    const int width;
    const int height;
    const int rows;
    const int columns;
    const std::vector<std::vector<cell::Cell>> cells;
};
export void render(const Grid& grid, SDL_Renderer* renderer) {
    // グリッド全体のオフセット位置を基準に描画
    for (int row = 0; row < grid.rows; ++row) {
        for (int col = 0; col < grid.columns; ++col) {
            const cell::Cell& cell = grid.cells[row][col];

            // グリッドのオフセットを反映
            SDL_Rect rect;
            rect.x = static_cast<int>(grid.position.x + cell.x);
            rect.y = static_cast<int>(grid.position.y + cell.y);
            rect.w = static_cast<int>(cell.width);
            rect.h = static_cast<int>(cell.height);

            // セル描画（ここでは cell::render() に任せる）
            cell::render(cell, renderer);
        }
    }

    // グリッド枠線を描く（任意）
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect gridRect;
    gridRect.x = static_cast<int>(grid.position.x);
    gridRect.y = static_cast<int>(grid.position.y);
    gridRect.w = grid.width;
    gridRect.h = grid.height;
    SDL_RenderDrawRect(renderer, &gridRect);
}
}  // namespace grid
