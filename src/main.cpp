#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
import GlobalSetting;
import Game;

void main_loop_tick(void* arg) {
    Game* game = static_cast<Game*>(arg);
    static Uint32 last_time = SDL_GetTicks();
    Uint32 current_time = SDL_GetTicks();
    double delta_time = (current_time - last_time) / 1000.0;
    last_time = current_time;
    game->tick(delta_time);
}

int main(int argc, char* argv[]) {
    const auto global_setting = global_setting::GlobalSetting::instance();
    // Gameオブジェクトのスコープを明示的に限定
    {
        Game game(global_setting);  // ← ここでSDL初期化とウィンドウ/レンダラー生成を内部で行う

        // 初期化失敗時はループに入らず終了コードを返します
        if (!game.isInitialized()) {
            // ループ終了後、Gameのデストラクタで
            // SDL_DestroyRenderer と SDL_DestroyWindow が自動的に呼ばれます。
            // SDL_Quit(); // → これもGameデストラクタ内で呼びます
            return 1;
        }

#ifdef __EMSCRIPTEN__
        emscripten_set_main_loop_arg(main_loop_tick, &game, 0, 1);
#else
        while (game.isRunning()) {
            main_loop_tick(&game);
        }
#endif
    }
    return 0;
}
