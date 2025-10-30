module;
#include <SDL2/SDL_pixels.h>
#include <array>
export module TetriMino;
import Position2D;
import Cell;

namespace tetrimino {
export enum class TetriminoType { I, O, T, S, Z, J, L };
export enum class TetriminoStatus { Falling, Landed, Merged };
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

export struct Tetrimino {
    const SDL_Color color;
    const TetriminoType type;
    const TetriminoStatus status;
    const Position2D position;
};
}  // namespace tetrimino
