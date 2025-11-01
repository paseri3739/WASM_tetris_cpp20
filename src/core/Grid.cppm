module;
#include <SDL2/SDL.h>
#include <format>
#include <string>
#include <tl/expected.hpp>
#include <vector>
export module Grid;
import Position2D;
import Cell;
import GlobalSetting;

namespace grid {
export struct Grid {
    const std::string id;
    const Position2D position;
    const int width;
    const int height;
    const int rows;
    const int columns;
    const std::vector<std::vector<cell::Cell>> cells;

    // static を付与
    static tl::expected<Grid, std::string> create(std::string_view id, const Position2D& position,
                                                  int width, int height, int rows, int columns) {
        if (rows <= 0 || columns <= 0) {
            return tl::make_unexpected("rows and columns must be positive");
        }
        if (width <= 0 || height <= 0) {
            return tl::make_unexpected("width and height must be positive");
        }

        const auto setting = global_setting::GlobalSetting::instance();
        if (width % setting.cellWidth != 0 || height % setting.cellHeight != 0) {
            return tl::make_unexpected("grid dimensions must be divisible by cell size");
        }
        if (width / setting.cellWidth != columns || height / setting.cellHeight != rows) {
            return tl::make_unexpected("rows/columns must match grid size and cell size");
        }

        // ここが重要：後から代入しない。挿入時に構築する
        std::vector<std::vector<cell::Cell>> grid_cells;
        grid_cells.reserve(rows);

        for (int r = 0; r < rows; ++r) {
            std::vector<cell::Cell> row_cells;
            row_cells.reserve(columns);

            for (int c = 0; c < columns; ++c) {
                const int cell_x = position.x + c * setting.cellWidth;
                const int cell_y = position.y + r * setting.cellHeight;

                const auto cell_result =
                    cell::Cell::create(cell_x, cell_y, setting.cellWidth, setting.cellHeight);

                if (!cell_result.has_value()) {
                    return tl::make_unexpected("failed to create cell at (" + std::to_string(r) +
                                               ", " + std::to_string(c) +
                                               "): " + cell_result.error());
                }

                // 代入ではなく挿入（コピー／ムーブ「構築」）する
                row_cells.push_back(std::move(cell_result).value());
                // ↑ tl::expected の rvalue に対する value() は T&& を返すためムーブ可
                //   （ムーブ代入ではなくムーブ「構築」なので const メンバでも問題なし）
            }

            grid_cells.push_back(std::move(row_cells));
        }

        return Grid{std::string(id), position, width, height, rows, columns, std::move(grid_cells)};
    }

   private:
    // コンストラクタ
    Grid(std::string id, Position2D position, int width, int height, int rows, int columns,
         std::vector<std::vector<cell::Cell>> cells)
        : id(std::move(id)),
          position(position),
          width(width),
          height(height),
          rows(rows),
          columns(columns),
          cells(std::move(cells)) {}
};

export tl::expected<Position2D, std::string> get_cell_position(const Grid& grid, int row,
                                                               int column) {
    if (row < 0 || row >= grid.rows || column < 0 || column >= grid.columns) {
        return tl::make_unexpected("row or column out of bounds");
    }
    const cell::Cell& cell = grid.cells[row][column];
    return Position2D{cell.x, cell.y};
}

export void render(const Grid& grid, SDL_Renderer* renderer) {
    // グリッド全体のオフセット位置を基準に描画
    for (int row = 0; row < grid.rows; ++row) {
        for (int col = 0; col < grid.columns; ++col) {
            const cell::Cell& cell = grid.cells[row][col];

            // グリッドのオフセットを反映
            SDL_Rect rect;
            rect.x = grid.position.x + cell.x;
            rect.y = grid.position.y + cell.y;
            rect.w = cell.width;
            rect.h = cell.height;

            // セル描画（ここでは cell::render() に任せる）
            cell::render(cell, renderer);
        }
    }

    // グリッド枠線を描く（任意）
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect gridRect;
    gridRect.x = grid.position.x;
    gridRect.y = grid.position.y;
    gridRect.w = grid.width;
    gridRect.h = grid.height;
    SDL_RenderDrawRect(renderer, &gridRect);
}
}  // namespace grid
