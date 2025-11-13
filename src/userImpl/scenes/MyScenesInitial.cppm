
// ===========================
// File: MyScenes-Initial.ixx (exported module partition)
// ===========================
module;
#include <SDL2/SDL.h>
#include <tl/expected.hpp>

export module MyScenes:Initial;  // パーティション名

import SceneFramework;
import GlobalSetting;
import Input;
import TetrisRule; // ★ ここだけ import すればよい
import GameKey;
import :Core;  // 型(InitialData, Scene) を参照

export namespace my_scenes {

using scene_fw::Env;

// 初期シーン生成
inline tl::expected<Scene, std::string> make_initial(
    const std::shared_ptr<const global_setting::GlobalSetting>& gs) {
    InitialData s{};
    s.setting = gs;
    const auto world = tetris_rule::make_world(gs);
    if (!world) {
        return tl::make_unexpected("world initialization failed.");
    }
    s.world = world.value();

    return Scene{s};
}

// 更新
inline Scene update(const InitialData& s, const Env<global_setting::GlobalSetting>& env) {
    // 例：PAUSE で次のシーンへ
    const auto pause_key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (env.input.pressed(*pause_key)) {
        NextData next{};
        return Scene{next};
    }

    InitialData u = s;
    tetris_rule::step_world(u.world, env);
    if (tetris_rule::is_gameover(u.world)) {
        NextData next{};
        return Scene{next};
    }
    return Scene{std::move(u)};
}

// 描画
inline void render(const InitialData& s, SDL_Renderer* const renderer) {
    tetris_rule::render_world(s.world, renderer);
}

}  // namespace my_scenes
