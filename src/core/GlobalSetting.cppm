module;
export module GlobalSetting;
export namespace global_setting {

struct GlobalSetting {
    const int gridColumns;
    const int gridRows;
    const int cellWidth;
    const int cellHeight;
    const int canvasWidth;
    const int canvasHeight;
    const int frameRate;

    // 現在の設定を取得
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
