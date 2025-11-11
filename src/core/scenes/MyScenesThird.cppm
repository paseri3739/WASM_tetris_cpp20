
// ===========================
// File: MyScenes-Third.ixx (exported module partition)
// ===========================
module;
#include <SDL2/SDL.h>
#include <memory>
#include <tl/expected.hpp>

export module MyScenes:Third;  // パーティション名

import SceneFramework;
import GlobalSetting;
import Input;
import GameKey;
import TetrisRule; // make_initial の再呼び出し経路で必要
import :Core;
import :Initial;  // make_initial を呼ぶため

export namespace my_scenes {

using scene_fw::Env;

inline Scene update(const ThirdData& s, const Env<global_setting::GlobalSetting>& env) {
    const auto key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (env.input.pressed(*key)) {
        if (auto initial =
                make_initial(std::make_shared<global_setting::GlobalSetting>(env.setting)))
            return initial.value();
    }
    return Scene{s};
}

inline void render(const ThirdData&, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

}  // namespace my_scenes