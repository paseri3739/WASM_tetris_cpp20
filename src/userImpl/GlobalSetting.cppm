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
    const int canvasWidth;
    const int canvasHeight;
    const int frameRate;
    const double dropRate;

    // フォントキャッシュ
    FontPtr font;

    // 現在の設定を取得
    GlobalSetting(int columns, int rows, int cell_w, int cell_h, int fps, double drop_rate,
                  FontPtr font_)
        : gridColumns(columns),
          gridRows(rows),
          cellWidth(cell_w),
          cellHeight(cell_h),
          canvasWidth(columns * cell_w),
          canvasHeight(rows * cell_h),
          frameRate(fps),
          dropRate(drop_rate),
          font(std::move(font_)) {}

    // 必要ならアクセサ
    TTF_Font* get_font() const noexcept { return font.get(); }
};

}  // namespace global_setting
