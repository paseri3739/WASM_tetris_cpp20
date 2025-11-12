module;
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
export module SceneBox;
import SceneFramework;
import SceneNext;
import Io;

export namespace scenebox {

struct Concept {
    virtual ~Concept() = default;
    virtual std::unique_ptr<Concept> step(void* env_erased) = 0;  // 純粋遷移
    virtual IO<Unit> renderIO() const = 0;                        // 効果は IO に封入
};

template <class T, class Setting>
class Model final : public Concept {
   public:
    explicit Model(T value) : state_(std::move(value)) {}

    std::unique_ptr<Concept> step(void* env_erased) override {
        auto& env = *static_cast<const scene_fw::Env<Setting>*>(env_erased);
        auto v = update(state_, env);  // NextVariant<T>
        std::unique_ptr<Concept> replacement{};

        std::visit(
            [&](auto&& x) {
                using U = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<U, T>) {
                    state_ = std::move(x);  // 滞在（純粋データの新値）
                } else {
                    replacement =
                        std::move(x).template materialize_model<Setting, Concept, Model>();
                }
            },
            std::move(v));

        return replacement;
    }

    IO<Unit> renderIO() const override { return renderIO(state_); }

   private:
    T state_;
};

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
