
// ===========================
// File: MyScenes-Third.ixx (exported module partition)
// ===========================
module;
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
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

// Game Over 画面の描画
inline void render(const ThirdData& s, SDL_Renderer* const renderer,
                   const Env<global_setting::GlobalSetting>& env) {
    // 背景を赤でクリア
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);

    // キャッシュしているフォントを取得
    const auto& setting = env.setting;
    if (setting.font) {
        constexpr const char* text = "Game Over";

        // 文字色(ここでは白)
        SDL_Color color{255, 255, 255, 255};

        // UTF-8 文字列としてレンダリング
        SDL_Surface* surface = TTF_RenderUTF8_Blended(setting.font.get(), text, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                int tex_w = 0;
                int tex_h = 0;
                SDL_QueryTexture(texture, nullptr, nullptr, &tex_w, &tex_h);

                // 画面中央に配置
                SDL_Rect dst{};
                dst.w = tex_w;
                dst.h = tex_h;
                dst.x = (setting.gridAreaWidth - tex_w) / 2;
                dst.y = (setting.gridAreaHeight - tex_h) / 2;

                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    SDL_RenderPresent(renderer);
}

}  // namespace my_scenes
