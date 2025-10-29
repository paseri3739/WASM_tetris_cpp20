module;
#include <SDL2/SDL.h>

#include <iostream>
#include <memory>
#include <optional>
import GlobalSetting;
import Input;
import Grid;
import Cell;
export module Scene;

class IScene {
   public:
    virtual ~IScene() = default;
    virtual void update(double delta_time) = 0;
    virtual void process_input(const input::Input& input) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;
    /**
     * シーン遷移します
     * @return 次のシーン
     */
    virtual std::optional<std::unique_ptr<IScene>> take_scene_transition() = 0;

   protected:
    std::unique_ptr<IScene> pending_scene_;  // 次のシーンへの遷移要求を保持
};

namespace scene {

class NextScene final : public IScene {
   public:
    NextScene() {
        const auto setting = global_setting::GlobalSetting::instance();
        std::vector<std::vector<cell::Cell>> cells;
        for (int i = 0; i < setting.gridColumns; i++) {
            for (int j = 0; j < setting.gridRows; j++) {
            }
        }
        const grid::Grid grid = {
            "1",
            {0, 0},
            setting.canvasWidth,
            setting.canvasHeight,
            setting.gridRows,
            setting.gridColumns,
        };
    };
    void update(double delta_time) override {};
    void process_input(const input::Input& input) override {};
    void render(SDL_Renderer* renderer) override {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    };
    std::optional<std::unique_ptr<IScene>> take_scene_transition() override {
        return std::nullopt;
    };

   private:
    std::unique_ptr<grid::Grid> grid_;
};

export class InitialScene final : public IScene {
   public:
    InitialScene() {};
    void update(double delta_time) override {
        // nothing
    };
    void process_input(const input::Input& input) override {
        if (input.pressed(input::InputKey::PAUSE)) {
            pending_scene_ = std::make_unique<NextScene>();
        }
        input_ = std::make_shared<input::Input>(input);
    };
    void render(SDL_Renderer* renderer) override {
        // 背景を黒にクリア
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // 線の色（赤）
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

        // 三角形の3点を定義
        SDL_Point p1 = {320, 100};
        SDL_Point p2 = {220, 380};
        SDL_Point p3 = {420, 380};

        // 3本の線で三角形を描画
        SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, p3.x, p3.y);
        SDL_RenderDrawLine(renderer, p3.x, p3.y, p1.x, p1.y);

        // 描画内容を画面に反映
        SDL_RenderPresent(renderer);
    };
    std::optional<std::unique_ptr<IScene>> take_scene_transition() override {
        if (pending_scene_) {
            return std::move(pending_scene_);
        }
        return std::nullopt;
    };

   private:
    std::shared_ptr<const input::Input> input_;
};

/**
 * シーンの遷移を管理するクラスです
 */
export class SceneManager {
   public:
    SceneManager(std::unique_ptr<IScene> initial_scene)
        : current_scene_(std::move(initial_scene)) {}
    void update(double delta_time) {
        current_scene_->update(delta_time);
        // 遷移要求をpull
        if (auto opt = current_scene_->take_scene_transition()) {
            change_scene(std::move(*opt));
        }
    }
    void render(SDL_Renderer* renderer) { current_scene_->render(renderer); }
    void process_input(const input::Input& input) { current_scene_->process_input(input); }
    void apply_scene_change() {
        if (!next_scene_) {
            return;
        }
        current_scene_ = std::move(next_scene_);
        next_scene_.reset();
    };

   private:
    std::unique_ptr<IScene> current_scene_;
    std::unique_ptr<IScene> next_scene_;

    void change_scene(std::unique_ptr<IScene> next) {
        // nullptrは無視
        if (next) {
            next_scene_ = std::move(next);  // 次のフレームで適用する
        }
    };
};
}  // namespace scene