module;
#include <SDL2/SDL.h>
#include <memory>
export module Game;

export class Game {
   public:
    Game(SDL_Window* window, SDL_Renderer* renderer)
        : window_(window, SDL_DestroyWindow), renderer_(renderer, SDL_DestroyRenderer) {}
    bool isRunning() const { return running_; }
    void tick(double delta_time_seconds) {
        this->processInput();
        this->update(delta_time_seconds);
        this->render();
    }

   private:
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window_;
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer_;
    bool running_ = true;
    void update(double delta_time) {
        // TODO:
    }
    void render() {
        // TODO:
        // 背景を黒にクリア
        SDL_SetRenderDrawColor(renderer_.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer_.get());

        // 線の色（赤）
        SDL_SetRenderDrawColor(renderer_.get(), 255, 0, 0, 255);

        // 三角形の3点を定義
        SDL_Point p1 = {320, 100};
        SDL_Point p2 = {220, 380};
        SDL_Point p3 = {420, 380};

        // 3本の線で三角形を描画
        SDL_RenderDrawLine(renderer_.get(), p1.x, p1.y, p2.x, p2.y);
        SDL_RenderDrawLine(renderer_.get(), p2.x, p2.y, p3.x, p3.y);
        SDL_RenderDrawLine(renderer_.get(), p3.x, p3.y, p1.x, p1.y);

        // 描画内容を画面に反映
        SDL_RenderPresent(renderer_.get());
    }

    void processInput() {
        SDL_Event event;
        // イベントキューからすべてのイベントを処理
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    // 終了イベント（ウィンドウの×ボタンなど）
                    running_ = false;
                    break;

                case SDL_KEYDOWN:
                    // キー押下イベント
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running_ = false;  // ESCキーで終了
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    // マウスクリックイベント
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        // 左クリック時の処理
                        // 例: 座標を取得
                        int x = event.button.x;
                        int y = event.button.y;
                        SDL_Log("Mouse Left Click: (%d, %d)", x, y);
                    }
                    break;

                default:
                    break;
            }
        }
    }
};