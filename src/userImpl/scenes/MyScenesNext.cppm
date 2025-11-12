
// ===========================
// File: MyScenes-Next.ixx (exported module partition)
// ===========================
module;
#include <SDL2/SDL.h>

export module MyScenes:Next;  // パーティション名

import SceneFramework;
import GlobalSetting;
import Input;
import GameKey;
import :Core;

export namespace my_scenes {

using scene_fw::Env;

// --- Next / Third は従来通り最小 ---
inline Scene update(const NextData& s, const Env<global_setting::GlobalSetting>& env) {
    const auto key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (env.input.pressed(*key)) {
        ThirdData third{};
        return Scene{third};
    }
    return Scene{s};
}

inline void render(const NextData&, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

}  // namespace my_scenes
