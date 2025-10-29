module;
#include <SDL2/SDL.h>
#include <string>
#include <string_view>
#include <tl/expected.hpp>
import GlobalSetting;
export module Cell;

namespace cell {
// 状態
export enum class CellStatus { Empty, Moving, Filled };

// セル構造体（不変）
export struct Cell {
    const int x;
    const int y;
    const int width;
    const int height;
    const CellStatus status;
    static tl::expected<Cell, std::string> create(int x, int y, int width, int height,
                                                  CellStatus status = CellStatus::Empty) {
        const auto setting = global_setting::GlobalSetting::instance();
        if (width != setting.cellWidth) {
            return tl::make_unexpected("width must be equal to gloal setting");
        }
        if (height != setting.cellHeight) {
            return tl::make_unexpected("height must be equal to gloal setting");
        }
        return Cell{x, y, width, height, status};
    };

   private:
    Cell(int x, int y, int width, int height, CellStatus status)
        : x(x), y(y), width(width), height(height), status(status) {}
};

// 状態名を文字列に変換
export constexpr std::string_view to_string(CellStatus s) noexcept {
    switch (s) {
        case CellStatus::Empty:
            return "Empty";
        case CellStatus::Moving:
            return "Moving";
        case CellStatus::Filled:
            return "Filled";
    }
    return "Unknown";
}

/**
 * セルを描画します
 * @param cell
 * @param renderer
 */
export void render(const Cell& cell, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);  // 灰色

    // 四角形の描画領域を設定
    SDL_Rect rect;
    rect.x = cell.x;
    rect.y = cell.y;
    rect.w = cell.width;
    rect.h = cell.height;

    // 塗りつぶし矩形を描画
    SDL_RenderFillRect(renderer, &rect);

    // 枠線を黒で描画
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &rect);
}

// 遷移テーブル
export constexpr bool can_transition(CellStatus from, CellStatus to) noexcept {
    constexpr bool table[3][3] = {/* from Empty  */ {false, true, false},
                                  /* from Moving */ {true, false, true},
                                  /* from Filled */ {true, false, false}};
    return table[static_cast<int>(from)][static_cast<int>(to)];
}

// 遷移関数（例外を使わず tl::expected を返す）
export tl::expected<Cell, std::string> transition(const Cell& cell, CellStatus next) noexcept {
    if (!can_transition(cell.status, next)) {
        std::string err = "invalid transition: ";
        err += std::string(to_string(cell.status));
        err += " -> ";
        err += std::string(to_string(next));
        return tl::unexpected(std::move(err));
    }
    return Cell::create(cell.x, cell.y, cell.width, cell.height, next);
}

// 明示的ユーティリティ関数群
export tl::expected<Cell, std::string> lock(const Cell& cell) noexcept {
    return transition(cell, CellStatus::Filled);
}

export tl::expected<Cell, std::string> clear(const Cell& cell) noexcept {
    return transition(cell, CellStatus::Empty);
}

export tl::expected<Cell, std::string> begin_move(const Cell& cell) noexcept {
    return transition(cell, CellStatus::Moving);
}

}  // namespace cell
