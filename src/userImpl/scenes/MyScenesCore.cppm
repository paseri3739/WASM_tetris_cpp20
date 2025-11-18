// ===========================
// File: MyScenes-Core.ixx (exported module partition)
// ===========================
module;
#include <entt/entt.hpp>
#include <memory>
#include <variant>

export module MyScenes:Core;  // パーティション名

import GlobalSetting;
import TetrisRule;

export namespace my_scenes {

// シーン純粋データ(World を抱えるだけ)
struct GameSceneData {
    std::shared_ptr<const global_setting::GlobalSetting> setting;
    tetris_rule::World world;
};

struct NextData {};
struct ThirdData {};

using Scene = std::variant<GameSceneData, NextData, ThirdData>;

}  // namespace my_scenes
