// ===========================
// File: MyScenes-Core.ixx (exported module partition)
// ===========================
module;
#include <entt/entt.hpp>
#include <variant>

export module MyScenes:Core;  // パーティション名

import GlobalSetting;
import TetrisRule;

export namespace my_scenes {

// シーン純粋データ(World を抱えるだけ)
struct GameSceneData {
    tetris_rule::World world;
};

struct InitialSceneData {};
struct GameOverSceneData {};

using Scene = std::variant<GameSceneData, InitialSceneData, GameOverSceneData>;

}  // namespace my_scenes
