// ===========================
// File: MyScenes.ixx (primary interface unit)
// ===========================
module;
#include <SDL2/SDL.h>
#include <tl/expected.hpp>
#include <variant>

export module MyScenes;

// フレームワーク連携のために必要
import SceneFramework;
import GlobalSetting;

// シーン定義や各シーン固有の処理はパーティションに分割
export import :Core;     // 型と共通事項
export import :Initial;  // 初期シーン
export import :Next;     // 次のシーン
export import :Third;    // 三つ目のシーン

export namespace my_scenes {

using scene_fw::Env;

// フレームワーク連携
struct Impl {
    using Scene = my_scenes::Scene;

    static tl::expected<Scene, std::string> make_initial(
        const std::shared_ptr<const global_setting::GlobalSetting>& gs) {
        return my_scenes::make_initial(gs);
    }

    static Scene step(Scene current, const Env<global_setting::GlobalSetting>& env) {
        return std::visit([&](auto const& ss) -> Scene { return update(ss, env); }, current);
    }

    static void draw(const Scene& current, SDL_Renderer* const r) {
        std::visit([&](auto const& ss) { render(ss, r); }, current);
    }
};

}  // namespace my_scenes
