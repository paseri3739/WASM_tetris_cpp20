module;
#include <tl/expected.hpp>
export module TetrisRule;
import TetriMino;
import Position2D;
import GlobalSetting;
import Grid;
import SceneFramework;
namespace tetris_rule {
export constexpr int rows = 20;
export constexpr int columns = 10;

export enum class FailReason { OutOfBounds, Collision };

export tl::expected<tetrimino::Tetrimino, FailReason> drop(const tetrimino::Tetrimino& tetrimino,
                                                           const grid::Grid& grid,
                                                           const scene_fw::Env env) {
    // TODO:
    const auto new_position =
        Position2D(tetrimino.position.x, tetrimino.position.y + env.setting.cellHeight);
    const tetrimino::Tetrimino new_tetrimino =
        tetrimino::Tetrimino(tetrimino.type, tetrimino.status, tetrimino.direction, new_position);
    return new_tetrimino;
}

}  // namespace tetris_rule
