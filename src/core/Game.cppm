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
        // ポーリングはGameのみが担当し、他のモジュールは入力状態を受け取るだけにする
        const auto input = poll_input(input_);
        //  ゲーム全体の入力処理はここだけ
        if (input->pressed(input::InputKey::QUIT)) {
            running_ = false;
        }
        input_ = input;
        scene_manager_->process_input(*input);
    }

    /**
     * 抽象入力を取得する関数
     * @param previous_input 以前の入力状態(エッジ検出のために使用)
     * @return 新しい入力状態
     */
    std::shared_ptr<const input::Input> poll_input(
        std::shared_ptr<const input::Input> previous_input) {
        // 初回フレームなど、previous_input が null の場合に備える
        if (!previous_input) {
            previous_input = std::make_shared<const input::Input>();
        }

        auto input = std::make_shared<input::Input>(*previous_input);  // コピーして操作対象にする
        for (auto& [_, state] : input->key_states) {
            state.is_pressed = false;
            state.is_released = false;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                input->key_states[input::InputKey::QUIT].is_pressed = true;
                continue;
            }

            if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) continue;

            auto maybe_key = input::to_input_key(event.key.keysym.sym);
            if (!maybe_key.has_value()) continue;

            input::InputKey key = maybe_key.value();
            input::InputState& state = input->key_states[key];

            if (event.type == SDL_KEYDOWN) {
                if (!state.is_held) state.is_pressed = true;
                state.is_held = true;
            } else {
                state.is_held = false;
                state.is_released = true;
            }
        }

        return input;
    }
};
