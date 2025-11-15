module;
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <functional>  // 追加
#include <iostream>    // エラーログ用
#include <memory>
#include <vector>  // 追加
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

export module Game;
import SceneFramework; // 新規
import Input;

// =============================
// フレーム処理(Game 本体)
// =============================

export template <class Setting, class SceneImpl>
    requires scene_fw::SceneAPI<SceneImpl, Setting>
class Game final {
   public:
    // SDL 初期化成功後に SDL 依存のグローバル状態を含めて Setting を構築するファクトリ
    using SettingFactory =
        std::function<std::shared_ptr<const Setting>(SDL_Window*, SDL_Renderer*)>;

    // Setting をそのまま受け取るのではなく、SDL 初期化成功後に Setting を構築する関数を受け取る
    explicit Game(SettingFactory make_setting, int canvas_width, int canvas_height)
        : window_(nullptr, SDL_DestroyWindow),
          renderer_(nullptr, SDL_DestroyRenderer),
          input_(nullptr),
          setting_(nullptr) {
        // SDLの初期化
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            running_ = false;
            return;
        }

        // SDL_ttf 初期化
        if (TTF_WasInit() == 0 && TTF_Init() == -1) {
            std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
            running_ = false;
            return;
        }

        // ウィンドウの作成
        SDL_Window* raw_window =
            SDL_CreateWindow("SDL2 Triangle (Emscripten)", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, canvas_width, canvas_height, SDL_WINDOW_SHOWN);
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

        // ここで SDL 初期化成功後に Setting を構築する(SDL 依存のグローバル状態もここで作れる)
        setting_ = make_setting(window_.get(), renderer_.get());
        if (!setting_) {
            std::cerr << "Setting could not be created by factory!" << std::endl;
            running_ = false;
            initialized_ = false;
            return;
        }

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

        // --- このフレーム用の Env を 1 度だけ構築し、update / render で共用する ---
        scene_fw::Env<Setting> env{*input_, *setting_, delta_time_seconds};

        this->update(env);
        this->render(env);

        // --- FPS の集計と出力 ---
        fps_elapsed_seconds_ += delta_time_seconds;
        ++fps_frame_count_;

        // 1秒ごとに平均値を出す(多すぎるログを避ける)
        if (fps_elapsed_seconds_ >= 1.0) {
            const double fps = static_cast<double>(fps_frame_count_) / fps_elapsed_seconds_;
            const double ms_per_frame = 1000.0 / (fps > 0.0 ? fps : 1.0);

            // SDL のログ機構(emscripten でもブラウザコンソールに出ます)
            SDL_Log("FPS: %.2f  (%.2f ms/frame)", fps, ms_per_frame);

            // 次の区間のためにリセット
            fps_elapsed_seconds_ = 0.0;
            fps_frame_count_ = 0;
        }
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
    double fps_elapsed_seconds_ = 0.0;
    uint32_t fps_frame_count_ = 0;

    // --- 追加: Setting 更新パッチの一時保管場所 ---
    using SettingPatch = typename scene_fw::Env<Setting>::SettingPatch;
    std::vector<SettingPatch> pending_setting_patches_;

    // Env を受け取る形に変更
    void update(scene_fw::Env<Setting>& env) {
        // --- Env に更新予約のキュー関数をセット ---
        env.queue_setting_update = [this](SettingPatch patch) {
            pending_setting_patches_.push_back(std::move(patch));
        };

        // ロジックはすべて SceneImpl 側へ委譲(この段階では setting_ はまだ不変)
        scene_ = SceneImpl::step(std::move(scene_), env);

        // --- Game 側で適用タイミングを制御：update 終了時に一括適用 ---
        if (!pending_setting_patches_.empty()) {
            // 複数予約されている場合は順次適用(関数合成相当)
            for (auto& patch : pending_setting_patches_) {
                // null 安全: patch は必ず新しい共有ポインタを返す想定
                setting_ = patch(setting_);
            }
            pending_setting_patches_.clear();
        }
    }

    // Env を引数に追加
    void render(const scene_fw::Env<Setting>& env) {
        SceneImpl::draw(scene_, renderer_.get(), env);
    }

    void processInput() {
        // ポーリングはGameのみが担当し、他のモジュールは入力状態を受け取るだけにする
        const auto input = poll_input(input_);
        //  ゲーム全体の入力処理はここだけ
        //  SDL_Keycode をそのまま利用し、利用側で任意の enum にマッピング可能とする
        if (input->pressed(SDLK_q)) {
            running_ = false;
        }
        input_ = input;
        // scene 側への明示的な伝播は不要(Env に束ねて update に渡す)
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
    static int run(typename Game<Setting, SceneImpl>::SettingFactory make_setting, int canvas_width,
                   int canvas_height) {
#ifdef __EMSCRIPTEN__
        using GameT = Game<Setting, SceneImpl>;
        static GameT game{std::move(make_setting), canvas_width,
                          canvas_height};  // Emscriptenではプログラム終了まで生存させる

        if (!game.isInitialized()) {
            return 1;
        }

        emscripten_set_main_loop_arg(&GameRunner::main_loop<GameT>, &game, 0, 1);
        return 0;
#else
        using GameT = Game<Setting, SceneImpl>;
        GameT game{std::move(make_setting), canvas_width, canvas_height};

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
int run_game(typename Game<Setting, SceneImpl>::SettingFactory make_setting, int canvas_width,
             int canvas_height) {
    return GameRunner::run<Setting, SceneImpl>(std::move(make_setting), canvas_width,
                                               canvas_height);
}
