module;
#include <SDL2/SDL.h>
#include <array>
#include <cmath>
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <tl/expected.hpp>
#include <utility>
#include <vector>

export module TetrisRule;

// 依存除去: 以下3つの import を削除
// import TetriMino;
// import Position2D;
// import Cell;

import GlobalSetting;
import SceneFramework;
import GameKey;
import Input;

namespace tetris_rule {

// =============================
// エイリアス
// =============================
using scene_fw::Env;
template <class T>
using Result = tl::expected<T, std::string>;

// =============================
// ECS コンポーネント／リソース
// =============================

// 依存除去: テトリミノ関連を ECS 側に再定義
enum class PieceType { I, O, T, S, Z, J, L };
enum class PieceStatus { Falling, Landed, Merged };
enum class PieceDirection { North, East, South, West };

// 色ユーティリティ（元 TetriMino.to_color の置換）
constexpr SDL_Color to_color(PieceType type) noexcept {
    switch (type) {
        case PieceType::I:
            return SDL_Color{0, 255, 255, 255};  // シアン
        case PieceType::O:
            return SDL_Color{255, 255, 0, 255};  // 黄色
        case PieceType::T:
            return SDL_Color{128, 0, 128, 255};  // 紫
        case PieceType::S:
            return SDL_Color{0, 255, 0, 255};  // 緑
        case PieceType::Z:
            return SDL_Color{255, 0, 0, 255};  // 赤
        case PieceType::J:
            return SDL_Color{0, 0, 255, 255};  // 青
        case PieceType::L:
            return SDL_Color{255, 165, 0, 255};  // オレンジ
    }
    return SDL_Color{255, 255, 255, 255};
}

struct Position {
    int x{}, y{};
};

struct TetriminoMeta {
    PieceType type{};
    PieceDirection direction{};
    PieceStatus status{};
};

struct ActivePiece {};  // 操作対象
struct Gravity {
    double rate_cps{};
};  // cells/sec
struct FallAccCells {
    double cells{};
};  // cells
struct SoftDrop {
    bool held{false};
    double multiplier{10.0};
};
struct MoveIntent {
    int dx{0};
    int dy{0};
};
struct LockTimer {
    double sec{0.0};
};

constexpr double kLockDelaySec = 0.3;

// 盤面占有
enum class CellStatus : std::uint8_t { Empty, Filled };

// グリッド情報＋占有
struct GridResource {
    int rows{};
    int cols{};
    int cellW{};
    int cellH{};
    int origin_x{0};
    int origin_y{0};
    std::vector<CellStatus> occ;  // row-major

    inline int index(int r, int c) const noexcept { return r * cols + c; }
    inline SDL_Rect rect_rc(int r, int c) const noexcept {
        return SDL_Rect{origin_x + c * cellW, origin_y + r * cellH, cellW, cellH};
    }
};

// =============================
// 形状ヘルパ（ローカル定義）
// =============================

using Coord = std::pair<std::int8_t, std::int8_t>;

// 依存除去: 引数型を ECS の PieceType / PieceDirection に変更
static constexpr std::array<Coord, 4> get_cells_north_local(PieceType type) noexcept {
    switch (type) {
        case PieceType::I:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{1, 3}};
        case PieceType::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case PieceType::T:
            return {Coord{0, 1}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
        case PieceType::S:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 0}, Coord{1, 1}};
        case PieceType::Z:
            return {Coord{0, 0}, Coord{0, 1}, Coord{1, 1}, Coord{1, 2}};
        case PieceType::J:
            return {Coord{0, 0}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
        case PieceType::L:
            return {Coord{0, 2}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
    }
    return {};
}

static constexpr std::array<Coord, 4> get_cells_east_local(PieceType type) noexcept {
    switch (type) {
        case PieceType::I:
            return {Coord{0, 2}, Coord{1, 2}, Coord{2, 2}, Coord{3, 2}};
        case PieceType::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case PieceType::T:
            return {Coord{0, 1}, Coord{1, 1}, Coord{1, 2}, Coord{2, 1}};
        case PieceType::S:
            return {Coord{0, 1}, Coord{1, 1}, Coord{1, 2}, Coord{2, 2}};
        case PieceType::Z:
            return {Coord{0, 2}, Coord{1, 1}, Coord{1, 2}, Coord{2, 1}};
        case PieceType::J:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{2, 1}};
        case PieceType::L:
            return {Coord{0, 1}, Coord{1, 1}, Coord{2, 1}, Coord{2, 2}};
    }
    return {};
}

static constexpr std::array<Coord, 4> get_cells_south_local(PieceType type) noexcept {
    switch (type) {
        case PieceType::I:
            return {Coord{2, 0}, Coord{2, 1}, Coord{2, 2}, Coord{2, 3}};
        case PieceType::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case PieceType::T:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{2, 1}};
        case PieceType::S:
            return {Coord{1, 1}, Coord{1, 2}, Coord{2, 0}, Coord{2, 1}};
        case PieceType::Z:
            return {Coord{1, 0}, Coord{1, 1}, Coord{2, 1}, Coord{2, 2}};
        case PieceType::J:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{2, 2}};
        case PieceType::L:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{2, 0}};
    }
    return {};
}

static constexpr std::array<Coord, 4> get_cells_west_local(PieceType type) noexcept {
    switch (type) {
        case PieceType::I:
            return {Coord{0, 1}, Coord{1, 1}, Coord{2, 1}, Coord{3, 1}};
        case PieceType::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case PieceType::T:
            return {Coord{0, 1}, Coord{1, 0}, Coord{1, 1}, Coord{2, 1}};
        case PieceType::S:
            return {Coord{0, 0}, Coord{1, 0}, Coord{1, 1}, Coord{2, 1}};
        case PieceType::Z:
            return {Coord{0, 1}, Coord{1, 0}, Coord{1, 1}, Coord{2, 0}};
        case PieceType::J:
            return {Coord{0, 1}, Coord{1, 1}, Coord{2, 0}, Coord{2, 1}};
        case PieceType::L:
            return {Coord{0, 0}, Coord{0, 1}, Coord{1, 1}, Coord{2, 1}};
    }
    return {};
}

static constexpr std::array<Coord, 4> cells_for(PieceType type, PieceDirection dir) noexcept {
    switch (dir) {
        case PieceDirection::North:
            return get_cells_north_local(type);
        case PieceDirection::East:
            return get_cells_east_local(type);
        case PieceDirection::South:
            return get_cells_south_local(type);
        case PieceDirection::West:
            return get_cells_west_local(type);
    }
    return {};
}

// =============================
// ワールドハンドル（シーンから利用）
// =============================

export struct World {
    std::shared_ptr<entt::registry> registry;
    entt::entity grid_singleton{entt::null};
    entt::entity active{entt::null};
};

// =============================
// Systems
// =============================

static inline void inputSystem(entt::registry& r, const input::Input& in) {
    const auto down_key = game_key::to_sdl_key(game_key::GameKey::DOWN);
    auto view = r.view<ActivePiece, SoftDrop>();
    for (auto e : view) {
        auto& sd = view.get<SoftDrop>(e);
        sd.held = (down_key && in.held(*down_key));
        // 左右／回転は MoveIntent を別途積む（省略）
    }
}

static inline void gravitySystem(entt::registry& r, double dt) {
    constexpr int kMaxDropsPerFrame = 6;
    auto view = r.view<ActivePiece, Gravity, FallAccCells, SoftDrop>();
    for (auto e : view) {
        auto& g = view.get<Gravity>(e);
        auto& acc = view.get<FallAccCells>(e);
        auto& sd = view.get<SoftDrop>(e);

        const double rate = g.rate_cps * (sd.held ? sd.multiplier : 1.0);
        acc.cells += dt * rate;

        int steps = static_cast<int>(std::floor(acc.cells));
        steps = std::max(0, std::min(steps, kMaxDropsPerFrame));
        if (steps > 0) {
            auto& mi = r.get_or_emplace<MoveIntent>(e);
            mi.dy += steps;
            acc.cells -= steps;
        }
    }
}

static inline void resolveDropSystem(entt::registry& r, const GridResource& grid,
                                     const Env<global_setting::GlobalSetting>& env) {
    auto view = r.view<ActivePiece, Position, TetriminoMeta, MoveIntent>();
    for (auto e : view) {
        auto& pos = view.get<Position>(e);
        auto& meta = view.get<TetriminoMeta>(e);
        auto* mi = r.try_get<MoveIntent>(e);
        if (!mi) continue;

        int steps = mi->dy;
        r.remove<MoveIntent>(e);
        if (steps <= 0) continue;

        const int step_px = env.setting.cellHeight;
        const auto shape = cells_for(meta.type, meta.direction);

        auto can_place = [&](int px, int py) -> bool {
            for (auto [rr, cc] : shape) {
                const int col = (px - grid.origin_x) / grid.cellW + cc;
                const int row = (py - grid.origin_y) / grid.cellH + rr;
                if (col < 0 || col >= grid.cols || row < 0 || row >= grid.rows) return false;
                if (grid.occ[grid.index(row, col)] == CellStatus::Filled) return false;
            }
            return true;
        };

        for (int i = 0; i < steps; ++i) {
            const int ny = pos.y + step_px;
            if (!can_place(pos.x, ny)) {
                meta.status = PieceStatus::Landed;
                break;
            }
            pos.y = ny;
        }

        if (meta.status != PieceStatus::Falling) {
            r.get_or_emplace<LockTimer>(e).sec += env.dt;
        } else if (r.any_of<LockTimer>(e)) {
            r.remove<LockTimer>(e);
        }
    }
}

static inline void lockAndMergeSystem(entt::registry& r, GridResource& grid,
                                      const Env<global_setting::GlobalSetting>& env) {
    std::vector<entt::entity> to_fix;
    auto view = r.view<ActivePiece, Position, TetriminoMeta, LockTimer>();
    for (auto e : view) {
        auto& meta = view.get<TetriminoMeta>(e);
        auto& lt = view.get<LockTimer>(e);
        if (meta.status == PieceStatus::Falling) continue;
        if (lt.sec < kLockDelaySec) continue;
        to_fix.push_back(e);
    }

    for (auto e : to_fix) {
        auto& pos = r.get<Position>(e);
        auto& meta = r.get<TetriminoMeta>(e);

        std::array<Coord, 4> cells{};
        switch (meta.direction) {
            case PieceDirection::North:
                cells = get_cells_north_local(meta.type);
                break;
            case PieceDirection::East:
                cells = get_cells_east_local(meta.type);
                break;
            case PieceDirection::South:
                cells = get_cells_south_local(meta.type);
                break;
            case PieceDirection::West:
                cells = get_cells_west_local(meta.type);
                break;
        }
        for (auto [rr, cc] : cells) {
            const int col = (pos.x - grid.origin_x) / grid.cellW + cc;
            const int row = (pos.y - grid.origin_y) / grid.cellH + rr;
            if (0 <= row && row < grid.rows && 0 <= col && col < grid.cols) {
                grid.occ[grid.index(row, col)] = CellStatus::Filled;
            }
        }

        r.destroy(e);

        // 新規スポーン
        const int spawn_col = 3;
        const int spawn_row = 3;
        const int spawn_x = grid.origin_x + spawn_col * grid.cellW;
        const int spawn_y = grid.origin_y + spawn_row * grid.cellH;

        auto np = r.create();
        r.emplace<Position>(np, spawn_x, spawn_y);
        r.emplace<TetriminoMeta>(np, PieceType::Z, PieceDirection::West, PieceStatus::Falling);
        r.emplace<ActivePiece>(np);

        const double base_rate = (env.setting.dropRate > 0.0) ? (1.0 / env.setting.dropRate) : 0.0;
        r.emplace<Gravity>(np, base_rate);
        r.emplace<FallAccCells>(np, 0.0);
        r.emplace<SoftDrop>(np, false, 10.0);
    }
}

static inline void lineClearSystem(GridResource& grid) {
    int write = grid.rows - 1;
    for (int r0 = grid.rows - 1; r0 >= 0; --r0) {
        bool full = true;
        for (int c0 = 0; c0 < grid.cols; ++c0) {
            if (grid.occ[grid.index(r0, c0)] != CellStatus::Filled) {
                full = false;
                break;
            }
        }
        if (!full) {
            if (write != r0) {
                for (int c0 = 0; c0 < grid.cols; ++c0) {
                    grid.occ[grid.index(write, c0)] = grid.occ[grid.index(r0, c0)];
                }
            }
            --write;
        }
    }
    for (int r0 = write; r0 >= 0; --r0) {
        for (int c0 = 0; c0 < grid.cols; ++c0) {
            grid.occ[grid.index(r0, c0)] = CellStatus::Empty;
        }
    }
}

// =============================
// 外部公開 API
// =============================

// ワールド生成（Result 返しに変更）
export inline Result<World> make_world(std::shared_ptr<const global_setting::GlobalSetting> gs) {
    if (!gs) return tl::make_unexpected(std::string{"GlobalSetting is null"});

    World w{};
    w.registry = std::make_shared<entt::registry>();
    auto& r = *w.registry;
    const auto& cfg = *gs;

    // GridResource（singleton 的エンティティ）
    w.grid_singleton = r.create();
    auto& grid = r.emplace<GridResource>(w.grid_singleton);
    grid.rows = cfg.gridRows;
    grid.cols = cfg.gridColumns;
    grid.cellW = cfg.cellWidth;
    grid.cellH = cfg.cellHeight;
    grid.origin_x = 0;
    grid.origin_y = 0;
    grid.occ.assign(grid.rows * grid.cols, CellStatus::Empty);

    // アクティブピース
    const int spawn_col = 3;
    const int spawn_row = 3;
    const int spawn_x = grid.origin_x + spawn_col * grid.cellW;
    const int spawn_y = grid.origin_y + spawn_row * grid.cellH;

    w.active = r.create();
    r.emplace<Position>(w.active, spawn_x, spawn_y);
    r.emplace<TetriminoMeta>(w.active, PieceType::Z, PieceDirection::West, PieceStatus::Falling);
    r.emplace<ActivePiece>(w.active);

    const double base_rate = (cfg.dropRate > 0.0) ? (1.0 / cfg.dropRate) : 0.0;
    r.emplace<Gravity>(w.active, base_rate);
    r.emplace<FallAccCells>(w.active, 0.0);
    r.emplace<SoftDrop>(w.active, false, 10.0);

    return w;
}

// 1フレーム更新
export inline void step_world(World& w, const Env<global_setting::GlobalSetting>& env) {
    if (!w.registry) return;
    auto& r = *w.registry;
    auto* grid = r.try_get<GridResource>(w.grid_singleton);
    if (!grid) return;

    inputSystem(r, env.input);
    gravitySystem(r, env.dt);
    resolveDropSystem(r, *grid, env);
    lockAndMergeSystem(r, *grid, env);
    lineClearSystem(*grid);
}

// 描画
export inline void render_world(const World& w, SDL_Renderer* renderer) {
    if (!w.registry) return;
    auto& r = *w.registry;

    // 背景
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    // Grid
    if (auto* grid = r.try_get<GridResource>(w.grid_singleton)) {
        for (int row = 0; row < grid->rows; ++row) {
            for (int col = 0; col < grid->cols; ++col) {
                SDL_Rect rect = grid->rect_rc(row, col);

                // 塗り
                if (grid->occ[grid->index(row, col)] == CellStatus::Filled) {
                    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
                }
                SDL_RenderFillRect(renderer, &rect);

                // 枠
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(renderer, &rect);
            }
        }
        // 外枠
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect outer{grid->origin_x, grid->origin_y, grid->cols * grid->cellW,
                       grid->rows * grid->cellH};
        SDL_RenderDrawRect(renderer, &outer);
    }

    // ActivePiece 描画（TetriMino の描画を流用 -> ECS ローカルで置換）
    auto view = r.view<const ActivePiece, const Position, const TetriminoMeta>();
    for (auto e : view) {
        const auto& pos = view.get<const Position>(e);
        const auto& meta = view.get<const TetriminoMeta>(e);

        const auto* grid = r.try_get<GridResource>(w.grid_singleton);
        const int cw = grid ? grid->cellW : 30;
        const int ch = grid ? grid->cellH : 30;

        // 色設定
        const SDL_Color color = to_color(meta.type);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

        // 形状セル
        const auto cells = cells_for(meta.type, meta.direction);
        for (const auto& c : cells) {
            const int x = pos.x + static_cast<int>(c.second) * cw;
            const int y = pos.y + static_cast<int>(c.first) * ch;
            SDL_Rect rect = {x, y, cw, ch};
            SDL_RenderFillRect(renderer, &rect);
        }

        // 4x4 グリッド（元 render_grid_around 相当）
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        const int grid_w = cw * 4;
        const int grid_h = ch * 4;
        SDL_Rect frame = {pos.x, pos.y, grid_w, grid_h};
        SDL_RenderDrawRect(renderer, &frame);
        for (int c = 1; c <= 3; ++c) {
            const int x = pos.x + c * cw;
            SDL_RenderDrawLine(renderer, x, pos.y, x, pos.y + grid_h);
        }
        for (int r0 = 1; r0 <= 3; ++r0) {
            const int y = pos.y + r0 * ch;
            SDL_RenderDrawLine(renderer, pos.x, y, pos.x + grid_w, y);
        }
    }

    SDL_RenderPresent(renderer);
}

}  // namespace tetris_rule
