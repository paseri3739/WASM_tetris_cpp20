module;
#include <SDL2/SDL.h>
#include <iostream>
#include <memory>
#include <optional>
export module Scene;
import GlobalSetting;
import Input;
import Grid;
import Cell;
import TetriMino;
import Position2D;
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
    NextScene() {};
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
    InitialScene() {
        global_setting::GlobalSetting& setting = global_setting::GlobalSetting::instance();

        const auto grid =
            grid::Grid::create("initial_scene_grid", {0, 0}, setting.canvasWidth,
                               setting.canvasHeight, setting.gridRows, setting.gridColumns);

        if (grid.has_value()) {
            grid_ = std::make_unique<grid::Grid>(std::move(grid).value());
        } else {
            std::cerr << "Failed to create grid: " << grid.error() << std::endl;
        }

        const auto cell_pos = grid::get_cell_position(*grid_, 3, 3);
        if (!cell_pos) {
            std::cerr << "Failed to get cell position: " << cell_pos.error() << std::endl;
        }

        const auto tetrimino =
            tetrimino::Tetrimino(tetrimino::TetriminoType::Z, tetrimino::TetriminoStatus::Falling,
                                 tetrimino::TetriminoDirection::West,
                                 cell_pos.has_value() ? cell_pos.value() : Position2D{0, 0});

        tetrimino_ = std::make_unique<tetrimino::Tetrimino>(std::move(tetrimino));
    };
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
        // 背景を白にクリア
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        grid::render(*grid_, renderer);
        tetrimino::render(*tetrimino_, renderer);
        tetrimino::render_grid_around(*tetrimino_, renderer);
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
    std::unique_ptr<grid::Grid> grid_;
    std::unique_ptr<tetrimino::Tetrimino> tetrimino_;
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
