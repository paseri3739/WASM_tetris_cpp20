module;
#include <memory>
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
    static const GlobalSetting& instance() {
        ensure_initialized_();
        return *current_;
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

    // 元コードの「static bool pending_update_;」「static GlobalSetting& pending_instance_;」の
    // 役割を shared_ptr とフラグで実装し直す（参照ではなく所有を持つ）
    inline static std::shared_ptr<const GlobalSetting> current_{};

    static void ensure_initialized_() {
        if (!current_) {
            // 既定値（元コードに合わせる: 10,20,30,30,60)
            current_.reset(new GlobalSetting(10, 20, 30, 30, 60));
        }
    }
};

}  // namespace global_setting
