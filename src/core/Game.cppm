module;
#include <SDL2/SDL.h>
#include <memory>
export module Game;
import Input;
import Scene;

export class Game {
   public:
    Game(SDL_Window* window, SDL_Renderer* renderer,
         std::unique_ptr<scene::SceneManager> scene_manager)
        : window_(window, SDL_DestroyWindow),
          renderer_(renderer, SDL_DestroyRenderer),
          scene_manager_(std::move(scene_manager)) {}
    [[nodiscard]]
    bool isRunning() const {
        return running_;
    }
    void tick(double delta_time_seconds) {
        this->processInput();
        this->update(delta_time_seconds);
        this->render();
    }

   private:
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window_;
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer_;
    std::unique_ptr<scene::SceneManager> scene_manager_;
    std::shared_ptr<const input::Input> input_;
    bool running_ = true;
    void update(double delta_time) {
        // TODO:
        scene_manager_->update(delta_time);
        scene_manager_->apply_scene_change();
    }
    void render() { scene_manager_->render(renderer_.get()); }

    void processInput() {
        const auto input = input::poll_input(input_);
        //  ゲーム全体の入力処理はここだけ
        if (input->pressed(input::InputKey::QUIT)) {
            running_ = false;
        }
        input_ = input;
        scene_manager_->process_input(*input);
    }
};
