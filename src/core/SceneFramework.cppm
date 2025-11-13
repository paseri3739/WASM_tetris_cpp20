module;
#include <SDL2/SDL.h>
#include <functional>  // 追加
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

    // --- 追加: Setting のイミュータブル更新を予約するための API ---
    // Patch は「現在の設定ポインタ -> 次の設定ポインタ」を返す純関数
    using SettingPatch =
        std::function<std::shared_ptr<const Setting>(std::shared_ptr<const Setting>)>;

    // シーン側から更新を予約するためのキューイング関数
    // （Game が提供。Scene はここに Patch を投げるだけ）
    std::function<void(SettingPatch)> queue_setting_update;

    // 便利ヘルパ: 値→値の変換器を渡すだけで Patch 化して予約
    template <class Fn>
    void update_setting(Fn&& fn) const {
        // Fn:   const Setting& -> Setting （新しい値を返す純関数）
        // 注意: Setting が大きい場合は move 最適化が効くように設計してください
        if (!queue_setting_update) return;
        queue_setting_update([f = std::forward<Fn>(fn)](std::shared_ptr<const Setting> cur) {
            // cur が null の可能性は Game 側で排除している前提
            return std::make_shared<const Setting>(f(*cur));
        });
    }

    // 便利ヘルパ: 既に構築済みの新設定をそのまま差し替え予約
    void replace_setting(std::shared_ptr<const Setting> next) const {
        if (!queue_setting_update) return;
        queue_setting_update(
            [next = std::move(next)](std::shared_ptr<const Setting>) { return next; });
    }
};

// Impl が「Setting を使うシーン実装」であることを縛るコンセプト
template <class Impl, class Setting>
concept SceneAPI =
    requires(typename Impl::Scene& s, const typename Impl::Scene& cs, const Env<Setting>& env,
             SDL_Renderer* const r, std::shared_ptr<const Setting> setting_ptr) {
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
