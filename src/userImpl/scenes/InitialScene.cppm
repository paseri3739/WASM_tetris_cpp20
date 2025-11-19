
// ===========================
// File: MyScenes-Next.ixx (exported module partition)
// ===========================
module;
#include <SDL2/SDL.h>

#include <tl/expected.hpp>

export module MyScenes:Initial;  // パーティション名

import SceneFramework;
import GlobalSetting;
import Input;
import GameKey;
import :Core;
import :GameScene;

export namespace my_scenes {

using scene_fw::Env;

// 初期シーン生成
inline tl::expected<Scene, std::string> create_first_scene(
    const global_setting::GlobalSetting& gs) {
    InitialSceneData s;
    return Scene{s};
}

// --- Next / Third は従来通り最小 ---
inline Scene update(const InitialSceneData& s, const Env<global_setting::GlobalSetting>& env) {
    const auto key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (env.input.pressed(*key)) {
        const auto game_scene = my_scenes::create_game_scene(env);
        if (!game_scene) {
            return s;
        }
        return game_scene.value();
    }
    return Scene{s};
}

inline void render(const InitialSceneData&, SDL_Renderer* const renderer,
                   const Env<global_setting::GlobalSetting>& env) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

}  // namespace my_scenes
