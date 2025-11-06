module;
#include <SDL2/SDL.h>
#include <iostream>  // エラーログ用
#include <memory>
export module Game;
import Input;
import Scene;
import GlobalSetting;

// =============================
// フレーム処理（Game 本体）
// =============================
export class Game final {
   public:
    explicit Game()
        : window_(nullptr, SDL_DestroyWindow),
          renderer_(nullptr, SDL_DestroyRenderer),
          input_(nullptr),
          running_(true),
          initialized_(false),
          setting_(std::make_shared<global_setting::GlobalSetting>(
              10, 20, 30, 30, 60))  // 旧: Grid 依存に合わせて初期化

    {
        // SDLの初期化
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            running_ = false;
            return;
        }

        // ウィンドウの作成
        SDL_Window* raw_window = SDL_CreateWindow(
            "SDL2 Triangle (Emscripten)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            setting_->canvasWidth, setting_->canvasHeight, SDL_WINDOW_SHOWN);
        if (!raw_window) {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            running_ = false;
            SDL_Quit();
            return;
        }
        window_.reset(raw_window);

        SDL_Renderer* raw_renderer =
            SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_ACCELERATED);
        if (!raw_renderer) {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError()
                      << std::endl;
            window_.reset();
            SDL_Quit();
            running_ = false;
            return;
        }
        renderer_.reset(raw_renderer);

        // ここで variant ベースの初期シーンを構築
        scene_ = scene::Scene{scene::make_initial(setting_)};

        initialized_ = true;
    }

    ~Game() {
        renderer_.reset();
        window_.reset();
        SDL_Quit();
    }

    [[nodiscard]] bool isRunning() const { return running_; }
    [[nodiscard]] bool isInitialized() const { return initialized_; }

    void tick(double delta_time_seconds) {
        this->processInput();
        this->update(delta_time_seconds);
        this->render();
    }

   private:
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window_;
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer_;

    // variant 化した現在のシーン
    scene::Scene scene_;

    std::shared_ptr<const input::Input> input_;
    std::shared_ptr<const global_setting::GlobalSetting> setting_;
    bool running_ = true;
    bool initialized_ = false;

    void update(double delta_time) {
        // シーン遷移があれば適用。ロジックはシーンに移譲し、各シーンが次のシーンを決定する。
        // → variant 方式では step() が常に次の Scene を返すので、そのまま差し替える。
        scene::Env env{*input_, *setting_, delta_time};
        scene_ = scene::step(std::move(scene_), env);
    }

    void render() { scene::draw(scene_, renderer_.get()); }

    void processInput() {
        // ポーリングはGameのみが担当し、他のモジュールは入力状態を受け取るだけにする
        const auto input = poll_input(input_);
        //  ゲーム全体の入力処理はここだけ
        if (input->pressed(input::InputKey::QUIT)) {
            running_ = false;
        }
        input_ = input;
        // scene 側への明示的な伝播は不要（Env に束ねて update に渡す）
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
