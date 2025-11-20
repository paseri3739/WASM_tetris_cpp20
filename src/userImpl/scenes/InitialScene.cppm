module;
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <tl/expected.hpp>

export module MyScenes:Initial;  // パーティション名
import SDLPtr;
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
    static int blink_counter = 0;
    constexpr int blink_interval = 30;  // 点滅の間隔（フレーム数）
    // 背景を赤でクリア
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);

    // キャッシュしているフォントを取得
    const auto& setting = env.setting;
    if (setting.font) {
        constexpr const char* text = "TETRIS";
        constexpr const char* subtext = "Press ENTER to Start";

        // 文字色(ここでは黒)
        SDL_Color color{0, 0, 0, 255};

        // UTF-8 文字列としてレンダリング
        SurfacePtr surface(TTF_RenderUTF8_Blended(setting.font.get(), text, color),
                           SDL_FreeSurface);
        if (surface) {
            TexturePtr texture(SDL_CreateTextureFromSurface(renderer, surface.get()),
                               SDL_DestroyTexture);
            if (texture) {
                int tex_w = 0;
                int tex_h = 0;
                SDL_QueryTexture(texture.get(), nullptr, nullptr, &tex_w, &tex_h);
                // 画面中央に配置
                SDL_Rect dst{
                    .x = (setting.canvasWidth - tex_w) / 2,
                    .y = (setting.canvasHeight / 4) - (tex_h / 2),
                    .w = tex_w,
                    .h = tex_h,
                };
                SDL_RenderCopy(renderer, texture.get(), nullptr, &dst);
            }
        }

        // サブテキストのレンダリング
        SurfacePtr sub_surface(TTF_RenderUTF8_Blended(setting.font.get(), subtext, color),
                               SDL_FreeSurface);
        if (sub_surface) {
            TexturePtr sub_texture(SDL_CreateTextureFromSurface(renderer, sub_surface.get()),
                                   SDL_DestroyTexture);
            if (sub_texture && !(blink_counter < blink_interval)) {
                int tex_w = 0;
                int tex_h = 0;
                SDL_QueryTexture(sub_texture.get(), nullptr, nullptr, &tex_w, &tex_h);
                // 画面中央下部に配置
                SDL_Rect dst{
                    .x = (setting.canvasWidth - tex_w) / 2,
                    .y = (setting.canvasHeight * 2 / 4) - (tex_h / 2),
                    .w = tex_w,
                    .h = tex_h,
                };
                SDL_RenderCopy(renderer, sub_texture.get(), nullptr, &dst);
            }
        }
    }
    blink_counter = (blink_counter + 1) % (blink_interval * 2);
    SDL_RenderPresent(renderer);
}

}  // namespace my_scenes
