module;
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module SceneNext;

export template <class T>
struct next_scenes;

// 遷移先 U をその場で構築せず、あとで Model<U,Setting> を作るための軽量ラッパ
export template <class U>
struct DeferNext {
    // U の実体は型消去して保持（生成時は U は完全型である前提：ユーザー側 TU で作る）
    std::shared_ptr<void> payload;  // 実体は shared_ptr<U> を void に消す

    // ヘルパ：U の値から DeferNext<U> を作る
    template <class... Args>
    static DeferNext make_from_value(Args&&... args) {
        auto p = std::make_shared<U>(std::forward<Args>(args)...);
        return DeferNext{std::move(p)};
    }

    // 後段（SceneBox 側）から呼ぶ：Model<U,Setting> を生成
    template <class Setting, class ConceptT, template <class, class> class ModelT>
    std::unique_ptr<ConceptT> materialize_model() && {
        auto up = std::static_pointer_cast<U>(std::move(payload));
        return std::unique_ptr<ConceptT>(new ModelT<U, Setting>(std::move(*up)));
    }
};

// NextVariant<T> を `U` ではなく `DeferNext<U>` の直和に変更
export template <class T>
struct next_variant {
    using next_tuple = typename next_scenes<T>::type;  // std::tuple<Next1, Next2, ...>

    template <std::size_t... I>
    static auto make(std::index_sequence<I...>)
        -> std::variant<T, DeferNext<std::tuple_element_t<I, next_tuple>>...>;

    using type = decltype(make(std::make_index_sequence<std::tuple_size_v<next_tuple>>{}));
};

export template <class T>
using NextVariant = typename next_variant<T>::type;

// ユーザーが使う簡便ヘルパ（U の値から DeferNext<U> を生成）
export template <class U, class... Args>
auto next(Args&&... args) {
    return DeferNext<U>::make_from_value(std::forward<Args>(args)...);
}
