module;
#include <SDL2/SDL.h>
#include <memory>
#include <tl/expected.hpp>
export module SceneFramework;
import Input;
import GlobalSetting;

export namespace scene_fw {

// フレームワークSTART
// フレーム毎に渡す不変の環境
// (元の Env をこちらに移すイメージ)
struct Env {
    const input::Input& input;
    const global_setting::GlobalSetting& setting;
    double dt;
};

// ユーザー側が満たすべきインターフェース (concept)
template <class Impl>
concept SceneAPI =
    requires(typename Impl::Scene& s, const typename Impl::Scene& cs, const Env& env,
             SDL_Renderer* r, std::shared_ptr<const global_setting::GlobalSetting> gs) {
        // Scene 型を持つこと
        typename Impl::Scene;

        // 初期シーン生成
        { Impl::make_initial(gs) } -> std::same_as<tl::expected<typename Impl::Scene, std::string>>;

        // 1ステップ更新
        { Impl::step(std::move(s), env) } -> std::same_as<typename Impl::Scene>;

        // 描画
        { Impl::draw(cs, r) } -> std::same_as<void>;
    };
// フレームワークEND

}  // namespace scene_fw
