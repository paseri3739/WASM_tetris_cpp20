module;
#include <SDL2/SDL.h>
#include <entt/entt.hpp>
#include <memory>
#include <tl/expected.hpp>

export module MyScenes;

import SceneFramework;
import GlobalSetting;
import Input;
import TetrisRule; // ★ ここだけ import すればよい
import GameKey;

export namespace my_scenes {

using scene_fw::Env;

// シーン純粋データ（World を抱えるだけ）
struct InitialData {
    std::shared_ptr<const global_setting::GlobalSetting> setting;
    tetris_rule::World world;
};

struct NextData {};
struct ThirdData {};
using Scene = std::variant<InitialData, NextData, ThirdData>;

// 初期シーン生成
inline tl::expected<Scene, std::string> make_initial(
    std::shared_ptr<const global_setting::GlobalSetting> gs) {
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
    if (pause_key && env.input.pressed(*pause_key)) {
        NextData next{};
        return Scene{next};
    }

    InitialData u = s;
    tetris_rule::step_world(u.world, env);
    return Scene{std::move(u)};
}

// 描画
inline void render(const InitialData& s, SDL_Renderer* renderer) {
    tetris_rule::render_world(s.world, renderer);
}

// --- Next / Third は従来通り最小 ---
inline Scene update(const NextData& s, const Env<global_setting::GlobalSetting>& env) {
    const auto key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (key && env.input.pressed(*key)) {
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

inline Scene update(const ThirdData& s, const Env<global_setting::GlobalSetting>& env) {
    const auto key = game_key::to_sdl_key(game_key::GameKey::PAUSE);
    if (key && env.input.pressed(*key)) {
        auto initial = make_initial(std::make_shared<global_setting::GlobalSetting>(env.setting));
        if (initial) return initial.value();
    }
    return Scene{s};
}

inline void render(const ThirdData&, SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

// フレームワーク連携
struct Impl {
    using Scene = my_scenes::Scene;

    static tl::expected<Scene, std::string> make_initial(
        std::shared_ptr<const global_setting::GlobalSetting> gs) {
        return my_scenes::make_initial(std::move(gs));
    }

    static Scene step(Scene current, const Env<global_setting::GlobalSetting>& env) {
        return std::visit([&](auto const& ss) -> Scene { return update(ss, env); }, current);
    }

    static void draw(const Scene& current, SDL_Renderer* r) {
        std::visit([&](auto const& ss) { render(ss, r); }, current);
    }
};

}  // namespace my_scenes
