#include <SDL2/SDL.h>  // SDL_Window, SDL_Renderer の型が必要
#include <memory>
import Game;
import GlobalSetting;
import MyScenes;

int main() {
    using Setting = global_setting::GlobalSetting;
    using Impl = my_scenes::Impl;

    // ウィンドウサイズはここで決める（必要なら Setting の内容から計算してもよい）
    constexpr int columns = 10;
    constexpr int rows = 20;
    constexpr int cell_width = 30;
    constexpr int cell_height = 30;
    constexpr int fps = 60;
    constexpr double drop_rate = 0.7;
    constexpr int canvas_width = columns * cell_width;
    constexpr int canvas_height = rows * cell_height;

    // SDL 初期化 / window / renderer 生成後に呼ばれる Setting のファクトリ
    Game<Setting, Impl>::SettingFactory factory =
        [=](SDL_Window* window, SDL_Renderer* renderer) -> std::shared_ptr<const Setting> {
        // もともと main に書いていた設定値をここに移す
        auto s = std::make_shared<Setting>(columns, rows, cell_width, cell_height, fps, drop_rate);

        // ここで SDL 依存のグローバル状態を構築して Setting に組み込める
        // 例:
        // s->init_with_sdl(window, renderer);

        // const 共有ポインタとして返す
        return std::shared_ptr<const Setting>(std::move(s));
    };

    // ユーザーは Impl 型を渡すだけ
    return run_game<Setting, Impl>(std::move(factory), canvas_width, canvas_height);
}
