module;
#include <SDL2/SDL.h>
#include <array>
#include <cmath>
#include <entt/entt.hpp>
#include <memory>
#include <utility>
#include <vector>

export module TetrisRule;

import TetriMino;
import Position2D;
import GlobalSetting;
import SceneFramework;
import GameKey;
import Input;

namespace tetris_rule {

// =============================
// エイリアス
// =============================
using scene_fw::Env;

// =============================
// ECS コンポーネント／リソース
// =============================

struct Position {
    int x{}, y{};
};

struct TetriminoMeta {
    tetrimino::TetriminoType type{};
    tetrimino::TetriminoDirection direction{};
    tetrimino::TetriminoStatus status{};
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

static constexpr std::array<Coord, 4> get_cells_north_local(
    tetrimino::TetriminoType type) noexcept {
    using TT = tetrimino::TetriminoType;
    switch (type) {
        case TT::I:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{1, 3}};
        case TT::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case TT::T:
            return {Coord{0, 1}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
        case TT::S:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 0}, Coord{1, 1}};
        case TT::Z:
            return {Coord{0, 0}, Coord{0, 1}, Coord{1, 1}, Coord{1, 2}};
        case TT::J:
            return {Coord{0, 0}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
        case TT::L:
            return {Coord{0, 2}, Coord{1, 0}, Coord{1, 1}, Coord{1, 2}};
    }
    return {};
}

static constexpr std::array<Coord, 4> get_cells_east_local(tetrimino::TetriminoType type) noexcept {
    using TT = tetrimino::TetriminoType;
    switch (type) {
        case TT::I:
            return {Coord{0, 2}, Coord{1, 2}, Coord{2, 2}, Coord{3, 2}};
        case TT::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case TT::T:
            return {Coord{0, 1}, Coord{1, 1}, Coord{1, 2}, Coord{2, 1}};
        case TT::S:
            return {Coord{0, 1}, Coord{1, 1}, Coord{1, 2}, Coord{2, 2}};
        case TT::Z:
            return {Coord{0, 2}, Coord{1, 1}, Coord{1, 2}, Coord{2, 1}};
        case TT::J:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{2, 1}};
        case TT::L:
            return {Coord{0, 1}, Coord{1, 1}, Coord{2, 1}, Coord{2, 2}};
    }
    return {};
}

static constexpr std::array<Coord, 4> get_cells_south_local(
    tetrimino::TetriminoType type) noexcept {
    using TT = tetrimino::TetriminoType;
    switch (type) {
        case TT::I:
            return {Coord{2, 0}, Coord{2, 1}, Coord{2, 2}, Coord{2, 3}};
        case TT::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case TT::T:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{2, 1}};
        case TT::S:
            return {Coord{1, 1}, Coord{1, 2}, Coord{2, 0}, Coord{2, 1}};
        case TT::Z:
            return {Coord{1, 0}, Coord{1, 1}, Coord{2, 1}, Coord{2, 2}};
        case TT::J:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{2, 2}};
        case TT::L:
            return {Coord{1, 0}, Coord{1, 1}, Coord{1, 2}, Coord{2, 0}};
    }
    return {};
}

static constexpr std::array<Coord, 4> get_cells_west_local(tetrimino::TetriminoType type) noexcept {
    using TT = tetrimino::TetriminoType;
    switch (type) {
        case TT::I:
            return {Coord{0, 1}, Coord{1, 1}, Coord{2, 1}, Coord{3, 1}};
        case TT::O:
            return {Coord{0, 1}, Coord{0, 2}, Coord{1, 1}, Coord{1, 2}};
        case TT::T:
            return {Coord{0, 1}, Coord{1, 0}, Coord{1, 1}, Coord{2, 1}};
        case TT::S:
            return {Coord{0, 0}, Coord{1, 0}, Coord{1, 1}, Coord{2, 1}};
        case TT::Z:
            return {Coord{0, 1}, Coord{1, 0}, Coord{1, 1}, Coord{2, 0}};
        case TT::J:
            return {Coord{0, 1}, Coord{1, 1}, Coord{2, 0}, Coord{2, 1}};
        case TT::L:
            return {Coord{0, 0}, Coord{0, 1}, Coord{1, 1}, Coord{2, 1}};
    }
    return {};
}

static constexpr std::array<Coord, 4> cells_for(tetrimino::TetriminoType type,
                                                tetrimino::TetriminoDirection dir) noexcept {
    using TD = tetrimino::TetriminoDirection;
    switch (dir) {
        case TD::North:
            return get_cells_north_local(type);
        case TD::East:
            return get_cells_east_local(type);
        case TD::South:
            return get_cells_south_local(type);
        case TD::West:
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
                meta.status = tetrimino::TetriminoStatus::Landed;
                break;
            }
            pos.y = ny;
        }

        if (meta.status != tetrimino::TetriminoStatus::Falling) {
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
        if (meta.status == tetrimino::TetriminoStatus::Falling) continue;
        if (lt.sec < kLockDelaySec) continue;
        to_fix.push_back(e);
    }

    for (auto e : to_fix) {
        auto& pos = r.get<Position>(e);
        auto& meta = r.get<TetriminoMeta>(e);

        std::array<Coord, 4> cells{};
        switch (meta.direction) {
            case tetrimino::TetriminoDirection::North:
                cells = get_cells_north_local(meta.type);
                break;
            case tetrimino::TetriminoDirection::East:
                cells = get_cells_east_local(meta.type);
                break;
            case tetrimino::TetriminoDirection::South:
                cells = get_cells_south_local(meta.type);
                break;
            case tetrimino::TetriminoDirection::West:
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
        r.emplace<TetriminoMeta>(np, tetrimino::TetriminoType::Z,
                                 tetrimino::TetriminoDirection::West,
                                 tetrimino::TetriminoStatus::Falling);
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

// ワールド生成
export inline World make_world(std::shared_ptr<const global_setting::GlobalSetting> gs) {
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
    r.emplace<TetriminoMeta>(w.active, tetrimino::TetriminoType::Z,
                             tetrimino::TetriminoDirection::West,
                             tetrimino::TetriminoStatus::Falling);
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

    // ActivePiece 描画（TetriMino の描画を流用）
    auto view = r.view<const ActivePiece, const Position, const TetriminoMeta>();
    for (auto e : view) {
        const auto& pos = view.get<const Position>(e);
        const auto& meta = view.get<const TetriminoMeta>(e);
        tetrimino::Tetrimino t{meta.type, meta.status, meta.direction, Position2D{pos.x, pos.y}};
        const auto* grid = r.try_get<GridResource>(w.grid_singleton);
        const int cw = grid ? grid->cellW : 30;
        const int ch = grid ? grid->cellH : 30;
        tetrimino::render(t, cw, ch, renderer);
        tetrimino::render_grid_around(t, renderer, cw, ch);
    }

    SDL_RenderPresent(renderer);
}

}  // namespace tetris_rule
