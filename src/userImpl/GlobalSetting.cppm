module;

#include <SDL2/SDL_ttf.h>
#include <memory>

export module GlobalSetting;
export namespace global_setting {

struct TtfFontDeleter {
    void operator()(TTF_Font* font) const noexcept {
        if (font) {
            TTF_CloseFont(font);
        }
    }
};

// SDL_ttf フォントを参照カウント付きで共有する
using FontPtr = std::shared_ptr<TTF_Font>;

struct GlobalSetting {
    const int gridColumns;
    const int gridRows;
    const int cellWidth;
    const int cellHeight;
    const int gridAreaWidth;
    const int gridAreaHeight;
    const int frameRate;
    const double dropRate;
    const int spawn_col = 3;          // スポーン列
    const int spawn_row = 3;          // スポーン行
    const double lockDelaySec = 0.3;  // ロック遅延時間(秒)
    const int maxDropsPerFrame = 6;   // 1フレームあたりの最大ドロップセル数
    const int maxRotationLocks = 15;  // 1回の回転操作あたりの最大ロック回数

    // フォントキャッシュ
    FontPtr font;

    // 現在の設定を取得
    GlobalSetting(int columns, int rows, int cell_w, int cell_h, int fps, double drop_rate,
                  FontPtr font_)
        : gridColumns(columns),
          gridRows(rows),
          cellWidth(cell_w),
          cellHeight(cell_h),
          gridAreaWidth(columns * cell_w),
          gridAreaHeight(rows * cell_h),
          frameRate(fps),
          dropRate(drop_rate),
          font(std::move(font_)) {}

    // 必要ならアクセサ
    TTF_Font* get_font() const noexcept { return font.get(); }
};

}  // namespace global_setting
