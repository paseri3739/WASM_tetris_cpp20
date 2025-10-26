#include <SDL2/SDL.h>
#include <iostream>
#include <tl/expected.hpp>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
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
    // SDLの初期化
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // ウィンドウの作成
    SDL_Window* window = SDL_CreateWindow("SDL2 Triangle (Emscripten)", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);

    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // レンダラーの作成
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    {  // Gameオブジェクトのスコープを明示的に限定
        Game game(window, renderer);

#ifdef __EMSCRIPTEN__
        // Emscriptenのメインループ登録（60fps）
        emscripten_set_main_loop_arg(main_loop_tick, &game, 0, 1);
#else
        while (game.isRunning()) {
            main_loop_tick(&game);
        }
#endif
        // gameのスコープを抜けるときにデストラクタが呼ばれる
    }
    // ループ終了後、Gameのデストラクタで
    // SDL_DestroyRenderer と SDL_DestroyWindow が自動的に呼ばれます。
    SDL_Quit();

    return 0;
}
