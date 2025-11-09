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
// ここで“引数の完全転送”を成立させ、コピーを避ける
template <class T, class... Args>
Command emplace_or_replace(entt::entity e, Args&&... args) {
    using Tup = std::tuple<std::decay_t<Args>...>;
    Tup pack{std::forward<Args>(args)...};  // ここで一度ムーブ/コピー確定
    return Command{[e, pack = std::move(pack)](entt::registry& r) mutable {
        std::apply(
            [&](auto&&... xs) {
                // pack の中身はこの時点ですべて右辺値として扱える
                r.emplace_or_replace<T>(e, std::forward<decltype(xs)>(xs)...);
            },
            std::move(pack));
    }};
}

// 値を直接渡すオーバーロードも用意（明示 move 向け）
template <class T>
Command emplace_or_replace(entt::entity e, T&& value) {
    auto holder = std::make_unique<std::decay_t<T>>(std::forward<T>(value));
    return Command{[e, holder = std::move(holder)](entt::registry& r) mutable {
        r.emplace_or_replace<std::decay_t<T>>(e, std::move(*holder));
    }};
}

template <class T>
Command remove(entt::entity e) {
    return Command{[=](entt::registry& r) {
        if (r.any_of<T>(e)) r.remove<T>(e);
    }};
}

inline Command destroy(entt::entity e) {
    return Command{[=](entt::registry& r) {
        if (r.valid(e)) r.destroy(e);
    }};
}

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
