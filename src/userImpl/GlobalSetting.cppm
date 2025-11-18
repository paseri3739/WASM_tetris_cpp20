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
    const int gridColumns;            // 列数
    const int gridRows;               // 行数
    const int cellWidth;              // セル幅(ピクセル)
    const int cellHeight;             // セル高さ(ピクセル)
    const int gridAreaWidth;          // グリッド領域幅(ピクセル)
    const int gridAreaHeight;         // グリッド領域高さ(ピクセル)
    const int frameRate;              // フレームレート(FPS)
    const double dropRate;            // 自動落下速度(セル／秒)
    const int spawn_col = 3;          // スポーン列
    const int spawn_row = 0;          // スポーン行
    const double lockDelaySec = 0.3;  // ロック遅延時間(秒)
    const int maxDropsPerFrame = 6;   // 1フレームあたりの最大ドロップセル数
    const int maxRotationLocks = 15;  // 1回の回転操作あたりの最大ロック回数
    const int nextAreaWidth = 150;    // Next表示領域幅(ピクセル)
    const int holdAreaWidth = 150;    // Hold表示領域幅(ピクセル)

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
