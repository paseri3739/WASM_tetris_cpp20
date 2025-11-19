
// ===========================
// File: MyScenes-Initial.ixx (exported module partition)
// ===========================
module;
#include <SDL2/SDL.h>
#include <tl/expected.hpp>

export module MyScenes:GameScene;  // パーティション名

import SceneFramework;
import GlobalSetting;
import Input;
import GameKey;
import :Core;  // 型(GameSceneData, Scene) を参照

export namespace my_scenes {

using scene_fw::Env;

// 初期シーン生成
inline tl::expected<Scene, std::string> create_game_scene(
    const Env<global_setting::GlobalSetting>& env) {
    GameSceneData s{
        .world = {},  // 後で初期化
    };
    const auto world = tetris_rule::make_world(env);
    if (!world) {
        return tl::make_unexpected("world initialization failed.");
    }
    s.world = world.value();

    return Scene{s};
}

// 更新
inline Scene update(const GameSceneData& s, const Env<global_setting::GlobalSetting>& env) {
    // 例：PAUSE で次のシーンへ
    // const auto pause_key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    // if (env.input.pressed(*pause_key)) {
    //     ThirdData next{};
    //     return Scene{next};
    // }

    GameSceneData u = s;
    tetris_rule::step_world(u.world, env);
    if (tetris_rule::is_gameover(u.world)) {
        GameOverSceneData next{};
        return Scene{next};
    }
    return Scene{std::move(u)};
}

// 描画
inline void render(const GameSceneData& s, SDL_Renderer* const renderer,
                   const Env<global_setting::GlobalSetting>& env) {
    tetris_rule::render_world(s.world, renderer, env);
}

}  // namespace my_scenes
