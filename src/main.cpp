#include <memory>
import Game;
import GlobalSetting;
import MyScenes;

int main() {
    auto setting = std::make_shared<const global_setting::GlobalSetting>(
        global_setting::GlobalSetting(10, 20, 30, 30, 60));

    // ユーザーは Impl 型を渡すだけ
    return run_game<global_setting::GlobalSetting, my_scenes::Impl>(setting);
}
