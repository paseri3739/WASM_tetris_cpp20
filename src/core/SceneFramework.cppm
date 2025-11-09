module;
#include <SDL2/SDL.h>
#include <memory>
#include <tl/expected.hpp>
export module SceneFramework;
import Input;
import GlobalSetting;
export namespace scene_fw {

// 任意の設定型 Setting を扱う Env
template <class Setting>
struct Env {
    const input::Input& input;
    const Setting& setting;
    double dt;
};

// Impl が「Setting を使うシーン実装」であることを縛るコンセプト
template <class Impl, class Setting>
concept SceneAPI =
    requires(typename Impl::Scene& s, const typename Impl::Scene& cs, const Env<Setting>& env,
             SDL_Renderer* r, std::shared_ptr<const Setting> setting_ptr) {
        // Scene 型を持つこと
        typename Impl::Scene;

        // 初期シーン生成
        {
            Impl::make_initial(setting_ptr)
        } -> std::same_as<tl::expected<typename Impl::Scene, std::string>>;

        // 1ステップ更新
        { Impl::step(std::move(s), env) } -> std::same_as<typename Impl::Scene>;

        // 描画
        { Impl::draw(cs, r) } -> std::same_as<void>;
    };

}  // namespace scene_fw
