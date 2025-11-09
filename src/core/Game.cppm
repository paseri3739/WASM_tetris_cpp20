module;
#include <SDL2/SDL.h>
#include <iostream>  // エラーログ用
#include <memory>
export module Game;
import SceneFramework; // 新規
import Input;

// =============================
// フレーム処理（Game 本体）
// =============================

export template <class Setting, class SceneImpl>
    requires scene_fw::SceneAPI<SceneImpl, Setting>
class Game final {
   public:
    explicit Game(std::shared_ptr<const Setting> setting)
        : window_(nullptr, SDL_DestroyWindow),
          renderer_(nullptr, SDL_DestroyRenderer),
          input_(nullptr),
          running_(true),
          initialized_(false),
          setting_(std::move(setting))  // 旧: Grid 依存に合わせて初期化
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

        // ここで ユーザー実装側の初期シーンを構築 (制御の反転)
        auto initial_scene = SceneImpl::make_initial(setting_);
        if (!initial_scene) {
            std::cerr << "Initial scene could not be created! reason: " << initial_scene.error()
                      << std::endl;
            initialized_ = false;
            return;
        }
        scene_ = std::move(initial_scene.value());

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

    // ユーザー定義 Scene 型
    typename SceneImpl::Scene scene_;

    std::shared_ptr<const input::Input> input_;
    std::shared_ptr<const Setting> setting_;  // ここが型パラメータ化
    bool running_ = true;
    bool initialized_ = false;

    void update(double delta_time) {
        scene_fw::Env<Setting> env{*input_, *setting_, delta_time};
        // ロジックはすべて SceneImpl 側へ委譲
        scene_ = SceneImpl::step(std::move(scene_), env);
    }

    void render() { SceneImpl::draw(scene_, renderer_.get()); }

    void processInput() {
        // ポーリングはGameのみが担当し、他のモジュールは入力状態を受け取るだけにする
        const auto input = poll_input(input_);
        //  ゲーム全体の入力処理はここだけ
        //  SDL_Keycode をそのまま利用し、利用側で任意の enum にマッピング可能とする
        if (input->pressed(SDLK_q)) {
            running_ = false;
        }
        input_ = input;
        // scene 側への明示的な伝播は不要（Env に束ねて update に渡す）
    }

    /**
     * 抽象入力を取得する関数
     * @param previous_input 以前の入力状態(エッジ検出のために使用)
     * @return 新しい入力状態
     *
     * SDL のキーコード(SDL_Keycode)をそのまま保持し、
     * 抽象キー(enum等)へのマッピングは利用側で行う方針とします。
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
                // ウィンドウクローズは直接ゲーム終了フラグに反映
                running_ = false;
                continue;
            }

            if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) continue;

            const SDL_Keycode code = event.key.keysym.sym;
            input::InputState& state = input->key_states[code];

            if (event.type == SDL_KEYDOWN) {
                if (!state.is_held) {
                    state.is_pressed = true;
                }
                state.is_held = true;
            } else {  // SDL_KEYUP
                if (state.is_held) {
                    state.is_held = false;
                    state.is_released = true;
                }
            }
        }

        return input;
    }
};

// =============================
// ランナー: main から呼ぶだけにする
// =============================
struct GameRunner {
#ifdef __EMSCRIPTEN__
    template <class GameT>
    static void main_loop(void* arg) {
        GameT* game = static_cast<GameT*>(arg);
        static Uint32 last_time = SDL_GetTicks();
        Uint32 current_time = SDL_GetTicks();
        double delta_time = (current_time - last_time) / 1000.0;
        last_time = current_time;
        game->tick(delta_time);
    }
#endif

    template <class Setting, class SceneImpl>
    static int run(std::shared_ptr<const Setting> setting) {
#ifdef __EMSCRIPTEN__
        using GameT = Game<Setting, SceneImpl>;
        static GameT game{std::move(setting)};  // Emscriptenではプログラム終了まで生存させる

        if (!game.isInitialized()) {
            return 1;
        }

        emscripten_set_main_loop_arg(&GameRunner::main_loop<GameT>, &game, 0, 1);
        return 0;
#else
        using GameT = Game<Setting, SceneImpl>;
        GameT game{std::move(setting)};

        if (!game.isInitialized()) {
            return 1;
        }

        Uint32 last_time = SDL_GetTicks();
        while (game.isRunning()) {
            Uint32 current_time = SDL_GetTicks();
            double delta_time = (current_time - last_time) / 1000.0;
            last_time = current_time;
            game.tick(delta_time);
        }

        return 0;
#endif
    }
};

// 利用側に公開するAPI
export template <class Setting, class SceneImpl>
    requires scene_fw::SceneAPI<SceneImpl, Setting>
int run_game(std::shared_ptr<const Setting> setting) {
    return GameRunner::run<Setting, SceneImpl>(std::move(setting));
}
