module;
#include <tl/expected.hpp>
export module TetrisRule;
import TetriMino;
import Position2D;
import GlobalSetting;
import Grid;
namespace tetris_rule {
export constexpr int rows = 20;
export constexpr int columns = 10;
// const auto setting = global_setting::GlobalSetting::instance();

// export enum class FailReason { OutOfBounds, Collision };

// export tl::expected<tetrimino::Tetrimino, FailReason> drop(const tetrimino::Tetrimino& tetrimino,
//                                                            const grid::Grid& grid) {
//     // TODO:
//     const auto new_position =
//         Position2D(tetrimino.position.x, tetrimino.position.y + setting.cellHeight);
//     return tetrimino;
// }

}  // namespace tetris_rule
