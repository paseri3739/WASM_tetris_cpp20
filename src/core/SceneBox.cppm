module;
#include <SDL2/SDL.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
export module SceneBox;
import SceneFramework;
import SceneNext;
import Io;

export namespace scenebox {

// ユーザーが用意する自由関数の要件：
//   update(const T&, const Env<Setting>&) -> NextVariant<T>
//   renderIO(const T&) -> IO<Unit>
// これらは ADL で見つかる前提（各シーンの namespace に置く）

// 型消去コンセプト
struct Concept {
    virtual ~Concept() = default;
    virtual std::unique_ptr<Concept> step(
        void* env_erased) = 0;  // 変化があれば新しい Concept を返す
    virtual IO<Unit> renderIO() const = 0;
};

// 具象モデル
template <class T, class Setting>
class Model final : public Concept {
   public:
    explicit Model(T value) : state_(std::move(value)) {}

    std::unique_ptr<Concept> step(void* env_erased) override {
        auto& env = *static_cast<const scene_fw::Env<Setting>*>(env_erased);
        // ADL によりシーン側の update を解決
        auto v = update(state_, env);  // NextVariant<T>
        std::unique_ptr<Concept> replacement{};

        std::visit(
            [&](auto&& x) {
                using U = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<U, T>) {
                    state_ = std::move(x);  // 滞在
                } else {
                    // 遷移：新しい Model<U> を生成（関数ポインタは不要）
                    replacement.reset(new Model<U, Setting>(std::move(x)));
                }
            },
            std::move(v));
        return replacement;
    }

    IO<Unit> renderIO() const override {
        // ADL によりシーン側の renderIO を解決
        return renderIO(state_);
    }

   private:
    T state_;
};

// ランタイムボックス
template <class Setting>
class Box {
   public:
    template <class T>
    static Box make(T init) {
        return Box{std::unique_ptr<Concept>(new Model<T, Setting>(std::move(init)))};
    }

    void step(const scene_fw::Env<Setting>& env) {
        if (auto rep = impl_->step((void*)&env)) impl_ = std::move(rep);
    }
    IO<Unit> renderIO() const { return impl_->renderIO(); }

   private:
    explicit Box(std::unique_ptr<Concept> p) : impl_(std::move(p)) {}
    std::unique_ptr<Concept> impl_;
};

}  // namespace scenebox
