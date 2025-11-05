module;

export module GlobalSetting;

namespace global_setting {
export struct GlobalSetting {
    const int gridColumns;
    const int gridRows;
    const int cellWidth;
    const int cellHeight;
    const int canvasWidth;
    const int canvasHeight;
    const int frameRate;

    static GlobalSetting& instance() {
        static GlobalSetting instance(10, 20, 30, 30, 60);
        return instance;
    }

    static void with(int columns, int rows, int cell_w, int cell_h, int fps) {
        GlobalSetting& setting = instance();
    }

   private:
    GlobalSetting(int columns, int rows, int cell_w, int cell_h, int fps)
        : gridColumns(columns),
          gridRows(rows),
          cellWidth(cell_w),
          cellHeight(cell_h),
          canvasWidth(columns * cell_w),
          canvasHeight(rows * cell_h),
          frameRate(fps) {}
};

}  // namespace global_setting
