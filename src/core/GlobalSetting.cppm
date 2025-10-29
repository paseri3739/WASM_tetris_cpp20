module;

export module GlobalSetting;

namespace global_setting {
export struct GlobalSetting {
    int gridColumns = 10;
    int gridRows = 20;
    int cellWidth = 30;
    int cellHeight = 30;
    int canvasWidth = 300;
    int canvasHeight = 600;
    int frameRate = 60;

    static GlobalSetting& instance() {
        static GlobalSetting instance;
        return instance;
    }

   private:
    GlobalSetting() {}
};
}  // namespace global_setting
