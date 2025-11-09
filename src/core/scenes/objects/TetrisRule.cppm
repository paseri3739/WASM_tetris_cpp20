module;
#include <tl/expected.hpp>
export module TetrisRule;
import TetriMino;
import Position2D;
import GlobalSetting;
import Grid;
import SceneFramework;
import GameKey;
namespace tetris_rule {
export constexpr int rows = 20;
export constexpr int columns = 10;

export enum class FailReason { OutOfBounds, Collision, NOT_IMPLEMENTED };

export tl::expected<tetrimino::Tetrimino, FailReason> drop(
    const tetrimino::Tetrimino& tetrimino, const grid::Grid& grid,
    const scene_fw::Env<global_setting::GlobalSetting> env) {
    // TODO:
    const auto new_position =
        Position2D(tetrimino.position.x, tetrimino.position.y + env.setting.cellHeight);
    if (new_position.x < 0) {
        return tl::make_unexpected(FailReason::OutOfBounds);
    }
    if (new_position.x >= grid.width) {
        return tl::make_unexpected(FailReason::Collision);
    }
    if (new_position.y < 0) {
        return tl::make_unexpected(FailReason::OutOfBounds);
    }
    if (new_position.y >= grid.height) {
        return tl::make_unexpected(FailReason::Collision);
    }
    const tetrimino::Tetrimino new_tetrimino =
        tetrimino::Tetrimino(tetrimino.type, tetrimino.status, tetrimino.direction, new_position);
    return new_tetrimino;
}

export tl::expected<tetrimino::Tetrimino, FailReason> move(
    const tetrimino::Tetrimino& tetrimino, const grid::Grid& grid,
    const scene_fw::Env<global_setting::GlobalSetting> env) {
    const auto key = game_key::to_sdl_key(game_key::GameKey::DOWN);
    if (!key) {
        return tetrimino;
    }

    // このフレームで "s" が押された瞬間だけ 1 マス落下させる
    if (env.input.pressed(*key)) {
        return drop(tetrimino, grid, env);
    }

    return tetrimino;
}

}  // namespace tetris_rule
