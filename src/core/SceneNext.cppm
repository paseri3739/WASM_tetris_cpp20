module;
#include <tuple>
#include <variant>
export module SceneNext;

// ユーザーは各シーン T に対して next_scenes<T> を特殊化して宣言する
export template <class T>
struct next_scenes;  // 未定義（ユーザー側で各シーンごとに特殊化）

// variant<T, Next1, Next2, ...> を自動生成
export template <class T>
struct next_variant {
    using next_tuple = typename next_scenes<T>::type;  // std::tuple<Next1,Next2,...>
    template <std::size_t... I>
    static auto make(std::index_sequence<I...>)
        -> std::variant<T, std::tuple_element_t<I, next_tuple>...>;
    using type = decltype(make(std::make_index_sequence<std::tuple_size_v<next_tuple>>{}));
};
export template <class T>
using NextVariant = typename next_variant<T>::type;
