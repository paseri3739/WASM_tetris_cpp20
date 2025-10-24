#include <SDL2/SDL.h>
#include <iostream>

int main(int argc, char *argv[]) {
  // SDLの初期化
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError()
              << std::endl;
    return 1;
  }

  // ウィンドウの作成
  SDL_Window *window =
      SDL_CreateWindow("SDL2 Triangle", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);

  if (!window) {
    std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError()
              << std::endl;
    SDL_Quit();
    return 1;
  }

  // レンダラーの作成
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError()
              << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  bool quit = false;
  SDL_Event e;

  while (!quit) {
    // イベント処理
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) {
        quit = true;
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

    SDL_Delay(16); // 約60fps
  }

  // 後始末
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
