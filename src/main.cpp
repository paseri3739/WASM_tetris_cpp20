#include <SDL2/SDL.h>
#include <iostream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
import hello_world;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
bool quit = false;

void main_loop() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            quit = true;
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();  // ループ終了（Emscripten）
#endif
            return;
        }
    }

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
}

int main(int argc, char* argv[]) {
    hello();                                            // モジュール関数の呼び出し
    hello2();                                           // モジュール関数の呼び出し
    std::cout << "3 + 5 = " << add(3, 5) << std::endl;  // add関数の呼び出し
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
    // Emscriptenのメインループ登録（60fps）
    emscripten_set_main_loop(main_loop, 60, 1);
#else
    // ネイティブのメインループ（約60fps相当）
    while (!quit) {
        main_loop();
        SDL_Delay(16);  // 60fps程度のスリープ
    }
#endif

    // （ループ終了後）
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
