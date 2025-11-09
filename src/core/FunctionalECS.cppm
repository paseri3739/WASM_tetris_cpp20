module;
#include <entt/entt.hpp>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
export module FunctionalECS;
namespace fecs {

// ============ 型ユーティリティ ============
template <class... Ts>
struct TypeList {
    using self = TypeList;
};

template <class TL, class T>
struct contains;
template <class... Ts, class T>
struct contains<TypeList<Ts...>, T> : std::bool_constant<(std::is_same_v<Ts, T> || ...)> {};

template <class A, class B>
struct intersect;
template <class... As, class... Bs>
struct intersect<TypeList<As...>, TypeList<Bs...>> {
    static constexpr bool value = ((contains<TypeList<As...>, Bs>::value) || ...);
};

// ★ 追加: TypeList を任意の可変長テンプレートに展開するためのユーティリティ
template <class TL>
struct typelist_apply;

template <class... Ts>
struct typelist_apply<TypeList<Ts...>> {
    template <template <class...> class C>
    using to = C<Ts...>;
};

// ============ 読み取りビュー ============
template <class... Rs>
class WorldView {
    const entt::registry* reg_{};

   public:
    explicit WorldView(const entt::registry& r) : reg_{&r} {}

    template <class... Ts>
    auto view() const {
        static_assert((contains<TypeList<Rs...>, std::remove_const_t<Ts>>::value && ...),
                      "WorldView: requesting undeclared read component");
        return reg_->view<Ts...>();
    }

    template <class T>
    const T& get(entt::entity e) const {
        static_assert(contains<TypeList<Rs...>, std::remove_const_t<T>>::value,
                      "WorldView: get() on undeclared read component");
        return reg_->get<const T>(e);
    }

    template <class T>
    const T* try_get(entt::entity e) const {
        static_assert(contains<TypeList<Rs...>, std::remove_const_t<T>>::value,
                      "WorldView: try_get() on undeclared read component");
        return reg_->try_get<const T>(e);
    }

    // どうしても registry 参照を渡したい箇所向けの隠し口（読み取り専用想定）
    const entt::registry& raw() const noexcept { return *reg_; }
};

// ============ 書き込みコマンド ============
struct CommandBase {
    std::function<void(entt::registry&)> apply;
};

template <class T>
struct CommandFor : CommandBase {
    using component = T;
};

namespace cmd {
template <class T, class... Args>
CommandFor<std::decay_t<T>> set(entt::entity e, Args&&... args) {
    using U = std::decay_t<T>;
    auto tup = std::make_tuple(std::forward<Args>(args)...);
    CommandFor<U> c;
    c.apply = [e, tup = std::move(tup)](entt::registry& r) mutable {
        std::apply(
            [&](auto&&... xs) { r.emplace_or_replace<U>(e, std::forward<decltype(xs)>(xs)...); },
            std::move(tup));
    };
    return c;
}

template <class T>
CommandFor<std::decay_t<T>> remove(entt::entity e) {
    using U = std::decay_t<T>;
    CommandFor<U> c;
    c.apply = [e](entt::registry& r) {
        if (r.any_of<U>(e)) r.remove<U>(e);
    };
    return c;
}

inline CommandBase destroy(entt::entity e) {
    CommandBase c;
    c.apply = [e](entt::registry& r) {
        if (r.valid(e)) r.destroy(e);
    };
    return c;
}
}  // namespace cmd

// ============ 書き込みバッファ（型制限） ============
template <class... Ws>
class CommandBuffer {
    std::vector<CommandBase> cs_;

   public:
    template <class T>
    void add(CommandFor<T> c) {
        static_assert(contains<TypeList<Ws...>, T>::value,
                      "CommandBuffer: trying to write an undeclared component type");
        cs_.push_back(std::move(c));
    }
    void add(CommandBase c) {  // destroy 等の型を持たないもの
        cs_.push_back(std::move(c));
    }
    void apply_all(entt::registry& r) {
        for (auto& c : cs_) c.apply(r);
        cs_.clear();
    }
};

// ============ Reads / Writes タグ ============
template <class... Ts>
struct Reads {
    using list = TypeList<std::remove_const_t<Ts>...>;
};
template <class... Ts>
struct Writes {
    using list = TypeList<std::remove_const_t<Ts>...>;
};

// ============ System ラッパと検査 ============
template <class RD, class WR, class Fn>
struct System {
    using reads = typename RD::list;
    using writes = typename WR::list;
    Fn fn;
};

template <class S>
concept PureSystemFn = requires(S s, const entt::registry& reg) {
    // 形だけの検査。呼び出し側で WorldView/CommandBuffer を構築して渡す
    // 実行シグネチャは run() 側で担保
    true;
};

// ============ フェーズ内の競合検査 ============
template <class... Sys>
struct Phase {
    std::tuple<Sys...> systems;

    // Write-Write / Write-Read 競合をコンパイル時に検査
    static consteval void check_conflicts() {
        constexpr std::size_t N = sizeof...(Sys);
        using Tuple = std::tuple<Sys...>;

        // ★ 修正: Is < J の全組合せを (I, J) で静的検査
        []<std::size_t... Is>(std::index_sequence<Is...>) {
            // 内部で (I, J) をチェックする関数
            auto pair_check = []<std::size_t I, std::size_t J>(
                                  std::integral_constant<std::size_t, I>,
                                  std::integral_constant<std::size_t, J>) {
                using Si = std::tuple_element_t<I, Tuple>;
                using Sj = std::tuple_element_t<J, Tuple>;

                constexpr bool w_w = intersect<typename Si::writes, typename Sj::writes>::value;
                constexpr bool w_r = intersect<typename Si::writes, typename Sj::reads>::value;
                static_assert(!(w_w || w_r),
                              "Phase conflict: writes clash (write-write or write-read). "
                              "Split into phases.");
            };

            // 各 I について J = I+1..N-1 を生成
            (
                []<std::size_t I>(std::integral_constant<std::size_t, I>) {
                    if constexpr (I + 1 < N) {
                        []<std::size_t... Js>(std::index_sequence<Js...>) {
                            (pair_check(std::integral_constant<std::size_t, I>{},
                                        std::integral_constant<std::size_t, (I + 1 + Js)>{}),
                             ...);
                        }(std::make_index_sequence<N - (I + 1)>{});
                    }
                }(std::integral_constant<std::size_t, Is>{}),
                ...);
        }(std::make_index_sequence<N>{});
    }
};

// ============ 実行 ============

// ★ 修正: TypeList から具体的な View / Buffer 型を生成し、1 本の run_system に集約
template <class RD, class WR, class Fn, class Resources>
void run_system(const entt::registry& reg, entt::registry& world, const Resources& res,
                const System<RD, WR, Fn>& sys) {
    using View = typename typelist_apply<typename RD::list>::template to<WorldView>;
    using OutCB = typename typelist_apply<typename WR::list>::template to<CommandBuffer>;

    static_assert(std::is_invocable_v<Fn, const View&, const Resources&, OutCB&>,
                  "System function signature must be: void(const WorldView<Rs...>&, "
                  "const Resources&, CommandBuffer<Ws...>&)");

    View view{reg};
    OutCB out;
    sys.fn(view, res, out);
    out.apply_all(world);
}

// スケジュール
template <class Resources, class... Phases>
struct Schedule {
    std::tuple<Phases...> phases;
};

template <class Resources, class... Phases>
void run_schedule(entt::registry& world, const Resources& res,
                  const Schedule<Resources, Phases...>& sch) {
    const entt::registry& ro = world;
    std::apply(
        [&](auto&... ph) {
            ((std::decay_t<decltype(ph)>::check_conflicts(),
              std::apply(
                  [&](auto&... sys) {
                      (run_system(ro, world, res, sys), ...);  // ★ 呼び出しを簡素化
                  },
                  ph.systems)),
             ...);
        },
        sch.phases);
}

}  // namespace fecs
