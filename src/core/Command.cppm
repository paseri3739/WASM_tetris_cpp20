module;
#include <entt/entt.hpp>
#include <functional>
#include <utility>
#include <vector>
export module Command;

// 汎用 Command：実行時に registry へ遅延適用する副作用
export struct Command {
    // 小粒ラムダで十分。パフォーマンスが必要なら小さな関数ラッパへ差し替え可
    std::function<void(entt::registry&)> apply;
};

// コマンドバッファ
export struct CommandBuffer {
    std::vector<Command> cmds;
    void add(Command c) { cmds.emplace_back(std::move(c)); }
    void clear() { cmds.clear(); }
    void apply_all(entt::registry& r) {
        for (auto& c : cmds) c.apply(r);
        cmds.clear();
    }
};

// よく使うコマンドのヘルパ（テンプレートで汎用化）
export namespace cmd {

// emplace_or_replace<T>(entity, args...)
template <class T, class... Args>
Command emplace_or_replace(entt::entity e, Args&&... args) {
    return Command{// ★ mutable を付けるだけ
                   [=](entt::registry& r) mutable {
                       r.emplace_or_replace<T>(e, std::forward<Args>(args)...);
                   }};
}

// remove<T>(entity)
template <class T>
Command remove(entt::entity e) {
    return Command{[=](entt::registry& r) {
        if (r.any_of<T>(e)) r.remove<T>(e);
    }};
}

// destroy(entity)
inline Command destroy(entt::entity e) {
    return Command{[=](entt::registry& r) {
        if (r.valid(e)) r.destroy(e);
    }};
}

// create() + 連鎖付与用の“生成フック”
inline Command create_then(std::function<void(entt::registry&, entt::entity)> f) {
    return Command{[f = std::move(f)](entt::registry& r) {
        const auto e = r.create();
        f(r, e);
    }};
}

}  // namespace cmd

// 純粋 System のシグネチャ：const registry + Resources -> CommandBuffer 追記のみ
template <class Resources>
using PureSystem = void (*)(const entt::registry& view, const Resources& res, CommandBuffer& out);

// フェーズ/スケジュール：競合を分けて逐次適用
export template <class Resources>
struct Phase {
    std::vector<PureSystem<Resources>> systems;
};

export template <class Resources>
struct Schedule {
    std::vector<Phase<Resources>> phases;
};

// 実行：各フェーズで System 群 → コマンド一括適用
export template <class Resources>
inline void run_schedule(entt::registry& world, const Resources& res,
                         const Schedule<Resources>& sch) {
    CommandBuffer buf;
    for (const auto& ph : sch.phases) {
        buf.clear();
        const entt::registry& view = world;  // 読み取り専用ビュー
        for (auto sys : ph.systems) sys(view, res, buf);
        buf.apply_all(world);  // 遅延副作用の境界
    }
}
