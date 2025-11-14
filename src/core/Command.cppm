module;
#include <entt/entt.hpp>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export module Command;

// ------------------------------
// 共通ユーティリティ
// ------------------------------
namespace detail {

// 型 T が List... のいずれかに含まれるかどうか
template <class T, class... List>
struct is_in : std::bool_constant<(std::is_same_v<T, List> || ...)> {};

template <class T, class... List>
inline constexpr bool is_in_v = is_in<T, List...>::value;

}  // namespace detail

// ------------------------------
// Command とコマンドユーティリティ
// ------------------------------

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

export using CommandList = std::vector<Command>;

// ------------------------------
// System 用の型安全ラッパ
// ------------------------------

// 指定されたコンポーネントだけ読み取れるビュー
export template <class... ReadComponents>
struct ReadOnlyView {
    const entt::registry& reg;

    // コンポーネント 1 つ取得
    template <class T>
    const T& get(entt::entity e) const {
        static_assert(detail::is_in_v<T, ReadComponents...>,
                      "System is not allowed to READ this component type");
        return reg.get<const T>(e);
    }

    // view を取得（指定できるのは ReadComponents のサブセットだけ）
    template <class... Ts>
    auto view() const {
        static_assert((detail::is_in_v<Ts, ReadComponents...> && ...),
                      "View contains a type that is not declared as READable");
        return reg.view<const Ts...>();
    }

    bool valid(entt::entity e) const { return reg.valid(e); }
};

// 指定されたコンポーネントだけを書き込めるコマンドファクトリ
export template <class... WriteComponents>
struct WriteCommands {
    // emplace_or_replace
    template <class T, class... Args>
    Command emplace_or_replace(entt::entity e, Args&&... args) const {
        static_assert(detail::is_in_v<T, WriteComponents...>,
                      "System is not allowed to WRITE this component type");
        return cmd::emplace_or_replace<T>(e, std::forward<Args>(args)...);
    }

    // remove
    template <class T>
    Command remove(entt::entity e) const {
        static_assert(detail::is_in_v<T, WriteComponents...>,
                      "System is not allowed to REMOVE this component type");
        return cmd::remove<T>(e);
    }

    // destroy はコンポーネント型に依存しないので常に許可するかどうかはポリシー次第
    Command destroy(entt::entity e) const { return cmd::destroy(e); }

    Command create_then(std::function<void(entt::registry&, entt::entity)> f) const {
        return cmd::create_then(std::move(f));
    }
};

// ------------------------------
// System, Phase, Schedule
// ------------------------------

// 純粋 System のシグネチャ：const registry + Resources -> CommandList
// （実体は「ReadOnlyView + WriteCommands + Resources -> CommandList」を包んだもの）
export template <class Resources>
using PureSystem = std::function<CommandList(const entt::registry& view, const Resources& res)>;

// フェーズ/スケジュール：競合を分けて逐次適用
export template <class Resources>
struct Phase {
    std::vector<PureSystem<Resources>> systems;
};

export template <class Resources>
struct Schedule {
    std::vector<Phase<Resources>> phases;
};

// System 実装を PureSystem に包むヘルパ
//   func: CommandList (ReadOnlyView<ReadComponents...>,
//                      WriteCommands<WriteComponents...>,
//                      const Resources&)
export template <class Resources, class... ReadComponents, class... WriteComponents>
PureSystem<Resources> make_system(CommandList (*func)(ReadOnlyView<ReadComponents...>,
                                                      WriteCommands<WriteComponents...>,
                                                      const Resources&)) {
    return [func](const entt::registry& reg, const Resources& res) -> CommandList {
        ReadOnlyView<ReadComponents...> ro{reg};
        WriteCommands<WriteComponents...> wr{};
        return func(ro, wr, res);
    };
}

// ------------------------------
// 実行：各フェーズで System 群 → コマンド一括適用
// ------------------------------
export template <class Resources>
inline void run_schedule(entt::registry& world, const Resources& res,
                         const Schedule<Resources>& sch) {
    for (const auto& ph : sch.phases) {
        CommandList buf;
        const entt::registry& view = world;
        for (const auto& sys : ph.systems) {
            buf = sys(view, res);  // RVO/ムーブ
            for (auto& c : buf) c.apply(world);
            buf.clear();
        }
    }
}
