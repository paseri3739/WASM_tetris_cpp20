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

constexpr std::array<std::array<cell::CellStatus, 4>, 4> get_shape_north(
    TetriminoType type) noexcept {
    constexpr auto Filled = cell::CellStatus::Filled;
    constexpr auto Empty = cell::CellStatus::Empty;
    switch (type) {
        case TetriminoType::I:
            return {{{Empty, Empty, Empty, Empty},
                     {Filled, Filled, Filled, Filled},
                     {Empty, Empty, Empty, Empty},
                     {Empty, Empty, Empty, Empty}}};
        case TetriminoType::O:
            return {{{Empty, Filled, Filled, Empty},
                     {Empty, Filled, Filled, Empty},
                     {Empty, Empty, Empty, Empty},
                     {Empty, Empty, Empty, Empty}}};
        case TetriminoType::T:
            return {{{Empty, Filled, Empty, Empty},
                     {Filled, Filled, Filled, Empty},
                     {Empty, Empty, Empty, Empty},
                     {Empty, Empty, Empty, Empty}}};
        case TetriminoType::S:
            return {{{Empty, Filled, Filled, Empty},
                     {Filled, Filled, Empty, Empty},
                     {Empty, Empty, Empty, Empty},
                     {Empty, Empty, Empty, Empty}}};
        case TetriminoType::Z:
            return {{{Filled, Filled, Empty, Empty},
                     {Empty, Filled, Filled, Empty},
                     {Empty, Empty, Empty, Empty},
                     {Empty, Empty, Empty, Empty}}};
        case TetriminoType::J:
            return {{{Filled, Empty, Empty, Empty},
                     {Filled, Filled, Filled, Empty},
                     {Empty, Empty, Empty, Empty},
                     {Empty, Empty, Empty, Empty}}};
        case TetriminoType::L:
            return {{{Empty, Empty, Filled, Empty},
                     {Filled, Filled, Filled, Empty},
                     {Empty, Empty, Empty, Empty},
                     {Empty, Empty, Empty, Empty}}};
    }
};

export struct Tetrimino {
    const TetriminoType type;
    const TetriminoStatus status;
    const Position2D position;
};
}  // namespace tetrimino
