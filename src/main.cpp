#include <SDL2/SDL.h>
#include <iostream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
import Game;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
bool quit = false;

void main_loop(Game& game) {
    // 前フレームの時刻を記録
    Uint32 last_time = SDL_GetTicks();

    while (game.isRunning()) {
        Uint32 current_time = SDL_GetTicks();
        double delta_time = (current_time - last_time) / 1000.0;  // 秒単位に変換
        last_time = current_time;

        game.tick(delta_time);

        // 60fps相当でスリープ（必要なら可変にしても良い）
        SDL_Delay(16);
    }
}

int main(int argc, char* argv[]) {
    // SDLの初期化
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // ウィンドウの作成
    window = SDL_CreateWindow("SDL2 Triangle (Emscripten)", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);

    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // レンダラーの作成
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

#ifdef __EMSCRIPTEN__
    Game game(window, renderer);
    const auto main_loop_bind = std::bind(main_loop, std::ref(game));
    // Emscriptenのメインループ登録（60fps）
    emscripten_set_main_loop(main_loop_bind, 60, 1);
#else
    Game game(window, renderer);

    // 前フレームの時刻を記録
    Uint32 last_time = SDL_GetTicks();

    while (game.isRunning()) {
        Uint32 current_time = SDL_GetTicks();
        double delta_time = (current_time - last_time) / 1000.0;  // 秒単位に変換
        last_time = current_time;

        game.tick(delta_time);

        // 60fps相当でスリープ（必要なら可変にしても良い）
        SDL_Delay(16);
    }
#endif

    // （ループ終了後）
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
