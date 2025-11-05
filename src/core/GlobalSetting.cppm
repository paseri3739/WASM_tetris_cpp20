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

    // 新しい設定値を登録（即時適用はしない）
    static void register_with(int columns, int rows, int cell_w, int cell_h, int fps) {
        ensure_initialized_();
        // std::make_shared は private ctor にアクセスできないため使用しない
        pending_instance_.reset(new GlobalSetting(columns, rows, cell_w, cell_h, fps));
        pending_update_ = true;
    }

    // register_with で登録した値を適用
    static void apply_pending_changes() {
        ensure_initialized_();
        if (pending_update_ && pending_instance_) {
            current_ = std::move(pending_instance_);
            pending_update_ = false;
        }
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
    inline static std::shared_ptr<const GlobalSetting> pending_instance_{};
    inline static bool pending_update_{false};

    static void ensure_initialized_() {
        if (!current_) {
            // 既定値（元コードに合わせる: 10,20,30,30,60)
            current_.reset(new GlobalSetting(10, 20, 30, 30, 60));
            pending_update_ = false;
            pending_instance_.reset();
        }
    }
};

}  // namespace global_setting
