module;
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module SceneNext;

export template <class T>
struct next_scenes;  // 各シーン T と同じパーティションで特殊化して宣言

// 遷移先 U を「あとで」実体化する軽量ラッパ
export template <class U>
struct DeferNext {
    std::shared_ptr<void> payload;  // 実体は shared_ptr<U> を void に消去

    template <class... Args>
    static DeferNext make_from_value(Args&&... args) {
        auto p = std::make_shared<U>(std::forward<Args>(args)...);
        return DeferNext{std::move(p)};
    }

    template <class Setting, class ConceptT, template <class, class> class ModelT>
    std::unique_ptr<ConceptT> materialize_model() && {
        auto up = std::static_pointer_cast<U>(std::move(payload));
        return std::unique_ptr<ConceptT>(new ModelT<U, Setting>(std::move(*up)));
    }
};

// `NextVariant<T>` = `variant<T, DeferNext<Next1>, DeferNext<Next2>, ...>`
export template <class T>
struct next_variant {
    using next_tuple = typename next_scenes<T>::type;
    template <std::size_t... I>
    static auto make(std::index_sequence<I...>)
        -> std::variant<T, DeferNext<std::tuple_element_t<I, next_tuple>>...>;
    using type = decltype(make(std::make_index_sequence<std::tuple_size_v<next_tuple>>{}));
};

export template <class T>
using NextVariant = typename next_variant<T>::type;

// ユーザー記述の簡便ヘルパ
export template <class U, class... Args>
auto next(Args&&... args) {
    return DeferNext<U>::make_from_value(std::forward<Args>(args)...);
}

// --- メタ：遷移許可の静的検査（ドキュメント化にも有用） ---
export namespace meta {
template <class T, class U, class Tuple>
struct contains;
template <class T, class U, class... Ts>
struct contains<T, U, std::tuple<Ts...>> : std::bool_constant<(std::is_same_v<U, Ts> || ...)> {};
template <class From, class To>
inline constexpr bool can_transition_v =
    contains<From, To, typename next_scenes<From>::type>::value;
}  // namespace meta
