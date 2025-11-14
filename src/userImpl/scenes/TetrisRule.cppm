module;
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <entt/entt.hpp>
#include <memory>
#include <random>
#include <string>
#include <tl/expected.hpp>
#include <utility>
#include <vector>

export module TetrisRule;

import GlobalSetting;
import SceneFramework;
import GameKey;
import Input;

// ★ 追加：フレームワーク側の純粋システム実行基盤を import
import Command;

namespace tetris_rule {

// =============================
// エイリアス
// =============================
using global_setting::GlobalSetting;
using scene_fw::Env;

// =============================
// ECS コンポーネント／リソース
// =============================

// テトリミノ関連enum
enum class PieceType { I, O, T, S, Z, J, L };
enum class PieceStatus { Falling, Landed, Merged };
enum class PieceDirection { North, East, South, West };

// 色ユーティリティ
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

struct RotateIntent {
    int dir{0};  // -1: 左回転, +1: 右回転
};

struct LockTimer {
    double sec{0.0};
};

constexpr double kLockDelaySec = 0.3;

struct PieceQueue {
    std::deque<PieceType> queue;
    std::mt19937 rng{std::random_device{}()};
};

static inline void refill_bag(PieceQueue& pq) {
    std::array<PieceType, 7> bag{PieceType::I, PieceType::O, PieceType::T, PieceType::S,
                                 PieceType::Z, PieceType::J, PieceType::L};
    std::shuffle(bag.begin(), bag.end(), pq.rng);
    for (auto t : bag) pq.queue.push_back(t);
}

static inline PieceType take_next(PieceQueue& pq) {
    if (pq.queue.empty()) refill_bag(pq);
    auto t = pq.queue.front();
    pq.queue.pop_front();
    return t;
}

struct HardDropRequest {};  // 押下フレームのみ付与

// 盤面占有
enum class CellStatus : std::uint8_t { Empty, Filled };

// ★ 追加：ゲームオーバー状態（Grid のシングルトンにぶら下げる）
struct GameOver {
    bool value{false};
};

// グリッド情報＋占有
struct GridResource {
    int rows{};
    int cols{};
    int cellW{};
    int cellH{};
    int origin_x{0};
    int origin_y{0};
    // ★ 追加：占有セルのテトリミノ種別（描画色復元用）
    // occ[index] == Filled のときのみ参照する
    std::vector<PieceType> occ_type;  // row-major, same size as occ
    std::vector<CellStatus> occ;      // row-major

    [[nodiscard]] inline int index(int row, int column) const noexcept {
        return row * cols + column;
    }
    [[nodiscard]] inline SDL_Rect rect_rc(int row, int column) const noexcept {
        return SDL_Rect{origin_x + column * cellW, origin_y + row * cellH, cellW, cellH};
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
// System 共通のリソース束ね（純粋化用）
// =============================

struct TetrisResources {
    const input::Input& input;
    const Env<GlobalSetting>& env;
    entt::entity grid_e{entt::null};
};

// =============================
// Systems（純粋版）
// =============================

// --- 追記: 方向遷移のヘルパ ---
static inline PieceDirection rotate_next(PieceDirection currentDirection, int dir /*-1 or +1*/) {
    if (dir == 0) return currentDirection;
    // East=+1, West=-1 相当で循環
    constexpr PieceDirection order[4] = {PieceDirection::North, PieceDirection::East,
                                         PieceDirection::South, PieceDirection::West};
    int idx = 0;
    switch (currentDirection) {
        case PieceDirection::North:
            idx = 0;
            break;
        case PieceDirection::East:
            idx = 1;
            break;
        case PieceDirection::South:
            idx = 2;
            break;
        case PieceDirection::West:
            idx = 3;
            break;
    }
    idx = (idx + (dir > 0 ? +1 : -1) + 4) % 4;
    return order[idx];
}

// =============================
// 入力（純粋）：意図を追加
// =============================
static CommandList inputSystem_pure(
    ReadOnlyView<ActivePiece, SoftDrop, MoveIntent, RotateIntent, HardDropRequest> ro,
    WriteCommands<SoftDrop, MoveIntent, RotateIntent, HardDropRequest> wr,
    const TetrisResources& res) {
    CommandList out;
    const auto down_key = game_key::to_sdl_key(game_key::GameKey::DOWN);
    auto v = ro.view<ActivePiece, SoftDrop>();

    auto moveView = ro.view<MoveIntent>();
    auto rotView = ro.view<RotateIntent>();

    for (auto e : v) {
        // SoftDrop held 更新（読み取り専用ビュー → “置換コマンド”）
        {
            const auto& sd = v.template get<SoftDrop>(e);
            SoftDrop next = sd;
            next.held = (down_key && res.input.held(*down_key));
            out.emplace_back(wr.emplace_or_replace<SoftDrop>(e, next));
        }

        // 左右／回転は MoveIntent / RotateIntent を押下瞬間のみ積む（元コメント維持）
        // --- 修正: 左右は「押下瞬間のみ」1セル分の MoveIntent を積む ---
        int dx = 0;
        if (const auto left = game_key::to_sdl_key(game_key::GameKey::LEFT);
            left && res.input.pressed(*left))
            dx -= 1;
        if (const auto right = game_key::to_sdl_key(game_key::GameKey::RIGHT);
            right && res.input.pressed(*right))
            dx += 1;

        if (dx != 0) {
            MoveIntent mi{};
            if (moveView.contains(e)) mi = moveView.template get<MoveIntent>(e);
            mi.dx += dx;
            out.emplace_back(wr.emplace_or_replace<MoveIntent>(e, mi));
        }
        // --- ここまで ---

        // --- 追記: 回転は押下瞬間のみ受理 ---
        int rot = 0;
        if (const auto rl = game_key::to_sdl_key(game_key::GameKey::ROTATE_LEFT);
            rl && res.input.pressed(*rl))
            rot -= 1;
        if (const auto rr = game_key::to_sdl_key(game_key::GameKey::ROTATE_RIGHT);
            rr && res.input.pressed(*rr))
            rot += 1;

        if (rot != 0) {
            RotateIntent ri{};
            if (rotView.contains(e)) ri = rotView.template get<RotateIntent>(e);
            ri.dir += (rot > 0 ? +1 : -1);  // 念のため -1..+1 に収める
            if (ri.dir > 1) ri.dir = 1;
            if (ri.dir < -1) ri.dir = -1;
            out.emplace_back(wr.emplace_or_replace<RotateIntent>(e, ri));
        }
        // --- 追記ここまで ---

        // --- ここから追記: ハードドロップ要求（押下瞬間のみ） ---
        if (const auto drop = game_key::to_sdl_key(game_key::GameKey::DROP);
            drop && res.input.pressed(*drop)) {
            out.emplace_back(wr.emplace_or_replace<HardDropRequest>(e));
        }
        // --- 追記ここまで ---
    }
    return out;
}

// =============================
// 重力（純粋）：FallAccCells と MoveIntent.dy を更新
// =============================
static CommandList gravitySystem_pure(
    ReadOnlyView<ActivePiece, Gravity, FallAccCells, SoftDrop, MoveIntent> ro,
    WriteCommands<FallAccCells, MoveIntent> wr, const TetrisResources& res) {
    CommandList out;
    auto v = ro.view<ActivePiece, Gravity, FallAccCells, SoftDrop>();
    auto moveView = ro.view<MoveIntent>();

    for (auto e : v) {
        constexpr int kMaxDropsPerFrame = 6;
        const auto& g = v.template get<Gravity>(e);
        const auto& sd = v.template get<SoftDrop>(e);

        FallAccCells acc = v.template get<FallAccCells>(e);
        const double rate = g.rate_cps * (sd.held ? sd.multiplier : 1.0);
        acc.cells += res.env.dt * rate;

        int steps = static_cast<int>(std::floor(acc.cells));
        steps = std::max(0, std::min(steps, kMaxDropsPerFrame));

        if (steps > 0) {
            MoveIntent mi{};
            if (moveView.contains(e)) mi = moveView.template get<MoveIntent>(e);
            mi.dy += steps;
            acc.cells -= steps;  // ← 発生分だけ減算して蓄積は維持

            out.emplace_back(wr.emplace_or_replace<MoveIntent>(e, mi));
        }

        // ← 重要: ステップの有無に関わらず、毎フレーム 蓄積を書き戻す
        out.emplace_back(wr.emplace_or_replace<FallAccCells>(e, acc));
    }
    return out;
}

// =============================
// 回転解決（純粋）
// --- 追記: 回転解決（クラシック／壁蹴りなし） ---
// =============================
static CommandList resolveRotationSystem_pure(
    ReadOnlyView<GridResource, ActivePiece, Position, TetriminoMeta, RotateIntent, LockTimer> ro,
    WriteCommands<RotateIntent, TetriminoMeta, LockTimer> wr, const TetrisResources& res) {
    CommandList out;
    const auto* grid = ro.valid(res.grid_e) ? &ro.get<GridResource>(res.grid_e) : nullptr;
    if (!grid) return out;

    auto v = ro.view<ActivePiece, Position, TetriminoMeta, RotateIntent>();
    for (auto e : v) {
        const auto& pos = v.template get<Position>(e);
        auto meta = v.template get<TetriminoMeta>(e);  // コピーして書換える
        const auto& ri = v.template get<RotateIntent>(e);
        out.emplace_back(wr.remove<RotateIntent>(e));  // 消費

        if (ri.dir == 0) continue;

        const PieceDirection ndir = (meta.type == PieceType::O)
                                        ? meta.direction
                                        : rotate_next(meta.direction, (ri.dir > 0 ? +1 : -1));

        const auto shape = cells_for(meta.type, ndir);
        auto can_place = [&](int px, int py) -> bool {
            for (auto [rr, cc] : shape) {
                const int col = (px - grid->origin_x) / grid->cellW + cc;
                const int row = (py - grid->origin_y) / grid->cellH + rr;
                if (col < 0 || col >= grid->cols || row < 0 || row >= grid->rows) return false;
                if (grid->occ[grid->index(row, col)] == CellStatus::Filled) return false;
            }
            return true;
        };

        // 壁蹴りなし: その場で置けるなら回転確定
        if (can_place(pos.x, pos.y)) {
            meta.direction = ndir;

            // 回転成功時はロック解除（クラシックな振る舞いの一つ）
            if (meta.status != PieceStatus::Falling) {
                meta.status = PieceStatus::Falling;
            }
            out.emplace_back(wr.emplace_or_replace<TetriminoMeta>(e, meta));
            out.emplace_back(wr.remove<LockTimer>(e));
        }
        // 置けない場合は不採用（何もしない）
    }
    return out;
}

// =============================
// 横移動解決（純粋）
// --- ここから追記: 横 → 縦 の順で解決 ---
// =============================
static CommandList resolveLateralSystem_pure(
    ReadOnlyView<GridResource, ActivePiece, Position, TetriminoMeta, MoveIntent> ro,
    WriteCommands<MoveIntent, Position> wr, const TetrisResources& res) {
    CommandList out;
    const auto* grid = ro.valid(res.grid_e) ? &ro.get<GridResource>(res.grid_e) : nullptr;
    if (!grid) return out;

    auto v = ro.view<ActivePiece, Position, TetriminoMeta, MoveIntent>();
    for (auto e : v) {
        auto pos = v.template get<Position>(e);
        const auto& meta = v.template get<TetriminoMeta>(e);
        const auto& mi = v.template get<MoveIntent>(e);

        int steps = mi.dx;
        // 水平方向ぶんはここで消費（垂直は resolveDrop へ委譲）
        if (mi.dx != 0) {
            MoveIntent next = mi;
            next.dx = 0;
            out.emplace_back(wr.emplace_or_replace<MoveIntent>(e, next));
        }
        if (steps == 0) continue;

        const int step_px = res.env.setting.cellWidth;
        const auto shape = cells_for(meta.type, meta.direction);

        auto can_place = [&](int px, int py) -> bool {
            for (auto [rr, cc] : shape) {
                const int col = (px - grid->origin_x) / grid->cellW + cc;
                const int row = (py - grid->origin_y) / grid->cellH + rr;
                if (col < 0 || col >= grid->cols || row < 0 || row >= grid->rows) return false;
                if (grid->occ[grid->index(row, col)] == CellStatus::Filled) return false;
            }
            return true;
        };

        const int dir = (steps > 0) ? +1 : -1;
        for (int i = 0; i < std::abs(steps); ++i) {
            const int nx = pos.x + dir * step_px;
            if (!can_place(nx, pos.y)) {
                // 壁/ブロックに当たる場合はそれ以上進めない（残りは破棄）
                break;
            }
            pos.x = nx;
        }
        out.emplace_back(wr.emplace_or_replace<Position>(e, pos));

        // 横移動ではロック状態は変更しない（縦落下系に委譲）
    }
    return out;
}

// =============================
// ロックタイマ加算（純粋）：着地中は毎フレーム dt を加算
// =============================
static CommandList lockTimerTickSystem_pure(ReadOnlyView<ActivePiece, TetriminoMeta, LockTimer> ro,
                                            WriteCommands<LockTimer> wr,
                                            const TetrisResources& res) {
    CommandList out;

    auto v = ro.view<ActivePiece, TetriminoMeta>();
    auto ltView = ro.view<LockTimer>();
    (void)res;

    for (auto e : v) {
        const auto& meta = v.template get<TetriminoMeta>(e);

        if (meta.status == PieceStatus::Falling) {
            // 落下中はロックタイマ不要
            out.emplace_back(wr.remove<LockTimer>(e));
            continue;
        }

        // --- 着地状態: 毎フレーム加算 ---
        LockTimer lt{};
        if (ltView.contains(e)) {
            lt = ltView.template get<LockTimer>(e);
        }
        lt.sec += res.env.dt;
        out.emplace_back(wr.emplace_or_replace<LockTimer>(e, lt));
    }
    return out;
}

// =============================
// 縦落下解決（純粋）
// =============================
static CommandList resolveDropSystem_pure(
    ReadOnlyView<GridResource, ActivePiece, Position, TetriminoMeta, MoveIntent> ro,
    WriteCommands<MoveIntent, Position, TetriminoMeta> wr, const TetrisResources& res) {
    CommandList out;
    const auto* grid = ro.valid(res.grid_e) ? &ro.get<GridResource>(res.grid_e) : nullptr;
    if (!grid) return out;

    auto v = ro.view<ActivePiece, Position, TetriminoMeta, MoveIntent>();
    for (auto e : v) {
        auto pos = v.template get<Position>(e);
        auto meta = v.template get<TetriminoMeta>(e);
        const auto& mi = v.template get<MoveIntent>(e);

        int steps = mi.dy;
        // 縦方向を消費
        out.emplace_back(wr.remove<MoveIntent>(e));
        if (steps <= 0) continue;

        const int step_px = res.env.setting.cellHeight;
        const auto shape = cells_for(meta.type, meta.direction);

        auto can_place = [&](int px, int py) -> bool {
            for (auto [rr, cc] : shape) {
                const int col = (px - grid->origin_x) / grid->cellW + cc;
                const int row = (py - grid->origin_y) / grid->cellH + rr;
                if (col < 0 || col >= grid->cols || row < 0 || row >= grid->rows) return false;
                if (grid->occ[grid->index(row, col)] == CellStatus::Filled) return false;
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

        // if (meta.status != PieceStatus::Falling) {
        //     // ロックタイマ加算
        //     LockTimer lt{};
        //     if (auto* old = view.try_get<LockTimer>(e)) {
        //         lt = *old;
        //     }
        //     lt.sec += res.env.dt;
        //     out.emplace_back(cmd::emplace_or_replace<LockTimer>(e, lt));
        // } else {
        //     out.emplace_back(cmd::remove<LockTimer>(e));
        // }

        out.emplace_back(wr.emplace_or_replace<Position>(e, pos));
        out.emplace_back(wr.emplace_or_replace<TetriminoMeta>(e, meta));
    }
    return out;
}

// =============================
// ハードドロップ（純粋）
// =============================
static CommandList hardDropSystem_pure(
    ReadOnlyView<GridResource, ActivePiece, Position, TetriminoMeta, HardDropRequest> ro,
    WriteCommands<Position, TetriminoMeta, LockTimer, HardDropRequest> wr,
    const TetrisResources& res) {
    CommandList out;
    const auto* grid = ro.valid(res.grid_e) ? &ro.get<GridResource>(res.grid_e) : nullptr;
    if (!grid) return out;

    auto v = ro.view<ActivePiece, Position, TetriminoMeta, HardDropRequest>();
    for (auto e : v) {
        auto pos = v.template get<Position>(e);
        auto meta = v.template get<TetriminoMeta>(e);

        const int step_py = res.env.setting.cellHeight;  // 1セルのピクセル
        const auto shape = cells_for(meta.type, meta.direction);

        auto can_place = [&](int px, int py) -> bool {
            for (auto [rr, cc] : shape) {
                const int col = (px - grid->origin_x) / grid->cellW + cc;
                const int row = (py - grid->origin_y) / grid->cellH + rr;
                if (col < 0 || col >= grid->cols || row < 0 || row >= grid->rows) return false;
                if (grid->occ[grid->index(row, col)] == CellStatus::Filled) return false;
            }
            return true;
        };

        // 可能な限り下へ
        int ny = pos.y;
        while (can_place(pos.x, ny + step_py)) ny += step_py;

        pos.y = ny;

        // 設置：即ロック扱い（次フレームで確実に Merge）
        meta.status = PieceStatus::Landed;
        LockTimer lt{};
        lt.sec = kLockDelaySec;

        out.emplace_back(wr.emplace_or_replace<Position>(e, pos));
        out.emplace_back(wr.emplace_or_replace<TetriminoMeta>(e, meta));
        out.emplace_back(wr.emplace_or_replace<LockTimer>(e, lt));
        out.emplace_back(wr.remove<HardDropRequest>(e));  // 消費
    }
    return out;
}

// =============================
// ロック & マージ（純粋）
// =============================
static CommandList lockAndMergeSystem_pure(
    ReadOnlyView<GridResource, ActivePiece, Position, TetriminoMeta, LockTimer> ro,
    WriteCommands<GridResource> wr, const TetrisResources& res) {
    CommandList out;
    if (!ro.valid(res.grid_e)) return out;
    auto grid = ro.get<GridResource>(res.grid_e);  // 書換え用コピー（最後に置換コマンドで反映）

    std::vector<entt::entity> to_fix;
    auto v = ro.view<ActivePiece, Position, TetriminoMeta, LockTimer>();
    for (auto e : v) {
        const auto& meta = v.template get<TetriminoMeta>(e);
        const auto& lt = v.template get<LockTimer>(e);
        if (meta.status == PieceStatus::Falling) continue;
        if (lt.sec < kLockDelaySec) continue;
        to_fix.push_back(e);
    }

    for (auto e : to_fix) {
        const auto& pos = v.template get<Position>(e);
        const auto& meta = v.template get<TetriminoMeta>(e);

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
                const int idx = grid.index(row, col);
                grid.occ[idx] = CellStatus::Filled;
                grid.occ_type[idx] = meta.type;  // ★ 追加：設置したピース種別を保持
            }
        }

        // アクティブピース破棄
        out.emplace_back(wr.destroy(e));

        // 新規スポーン（7-Bag）
        out.emplace_back(wr.create_then([&](entt::registry& r, entt::entity ne) {
            auto& g = r.get<GridResource>(res.grid_e);
            constexpr int spawn_col = 3;
            constexpr int spawn_row = 3;
            const int spawn_x = g.origin_x + spawn_col * g.cellW;
            const int spawn_y = g.origin_y + spawn_row * g.cellH;

            // registry のコンテキストに保存してある PieceQueue を使用
            auto& piece_queue = r.ctx().get<PieceQueue>();
            if (piece_queue.queue.empty()) refill_bag(piece_queue);
            const PieceType next_type = take_next(piece_queue);

            r.emplace<Position>(ne, spawn_x, spawn_y);
            r.emplace<TetriminoMeta>(ne, next_type, PieceDirection::West, PieceStatus::Falling);
            r.emplace<ActivePiece>(ne);

            const double base_rate =
                (res.env.setting.dropRate > 0.0) ? (1.0 / res.env.setting.dropRate) : 0.0;
            r.emplace<Gravity>(ne, base_rate);
            r.emplace<FallAccCells>(ne, 0.0);
            r.emplace<SoftDrop>(ne, false, 10.0);
        }));
    }

    // Grid 書換えを発行
    out.emplace_back(wr.emplace_or_replace<GridResource>(res.grid_e, grid));
    return out;
}

// =============================
// ライン消去（純粋）
// =============================
static CommandList lineClearSystem_pure(ReadOnlyView<GridResource> ro,
                                        WriteCommands<GridResource> wr,
                                        const TetrisResources& res) {
    CommandList out;
    if (!ro.valid(res.grid_e)) return out;
    auto grid = ro.get<GridResource>(res.grid_e);  // コピーを編集し、最後に置換

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
                    const int src = grid.index(r0, c0);
                    const int dst = grid.index(write, c0);
                    grid.occ[dst] = grid.occ[src];
                    grid.occ_type[dst] = grid.occ_type[src];  // ★ 追加：色も移動
                }
            }
            --write;
        }
    }
    for (int r0 = write; r0 >= 0; --r0) {
        for (int c0 = 0; c0 < grid.cols; ++c0) {
            const int idx = grid.index(r0, c0);
            grid.occ[idx] = CellStatus::Empty;
            // ★ 任意：既定値でクリア（未使用だが保守性のため）
            grid.occ_type[idx] = PieceType::I;
        }
    }

    out.emplace_back(wr.emplace_or_replace<GridResource>(res.grid_e, grid));
    (void)res;
    return out;
}

// =============================
// ゲームオーバー判定（純粋）
// --- 追記: 生成された ActivePiece が置けない場合にフラグを立てる ---
// =============================
static CommandList gameOverCheckSystem_pure(
    ReadOnlyView<GridResource, GameOver, ActivePiece, Position, TetriminoMeta> ro,
    WriteCommands<GameOver> wr, const TetrisResources& res) {
    CommandList out;

    const auto* grid = ro.valid(res.grid_e) ? &ro.get<GridResource>(res.grid_e) : nullptr;
    if (!grid) return out;

    // 既に GameOver 済みなら何もしない
    if (ro.valid(res.grid_e)) {
        const auto& go = ro.get<GameOver>(res.grid_e);
        if (go.value) return out;
    }

    auto v = ro.view<ActivePiece, Position, TetriminoMeta>();
    for (auto e : v) {
        const auto& pos = v.template get<Position>(e);
        const auto& meta = v.template get<TetriminoMeta>(e);

        const auto shape = cells_for(meta.type, meta.direction);

        auto can_place = [&](int px, int py) -> bool {
            for (auto [rr, cc] : shape) {
                const int col = (px - grid->origin_x) / grid->cellW + cc;
                const int row = (py - grid->origin_y) / grid->cellH + rr;
                if (col < 0 || col >= grid->cols || row < 0 || row >= grid->rows) return false;
                if (grid->occ[grid->index(row, col)] == CellStatus::Filled) return false;
            }
            return true;
        };

        if (!can_place(pos.x, pos.y)) {
            // ★ 追加：ゲームオーバーフラグを ON
            const auto gameover = GameOver{true};
            out.emplace_back(wr.emplace_or_replace<GameOver>(res.grid_e, gameover));
            // 以降のループは不要（単一アクティブ前提）。複数あっても一つで判定十分。
            break;
        }
    }
    return out;
}

// =============================
// 外部公開 API
// =============================

// ワールド生成
export inline tl::expected<World, std::string> make_world(
    const std::shared_ptr<const GlobalSetting>& gs) {
    if (!gs) return tl::make_unexpected(std::string{"GlobalSetting is null"});

    World world{};
    world.registry = std::make_shared<entt::registry>();
    auto& registry = *world.registry;
    const auto& cfg = *gs;

    // GridResource（singleton 的エンティティ）
    world.grid_singleton = registry.create();
    auto& grid = registry.emplace<GridResource>(world.grid_singleton);
    // ★ 追加：ゲームオーバーフラグ初期化
    registry.emplace<GameOver>(world.grid_singleton, GameOver{false});
    grid.rows = cfg.gridRows;
    grid.cols = cfg.gridColumns;
    grid.cellW = cfg.cellWidth;
    grid.cellH = cfg.cellHeight;
    grid.origin_x = 0;
    grid.origin_y = 0;
    grid.occ.assign(grid.rows * grid.cols, CellStatus::Empty);
    grid.occ_type.assign(grid.rows * grid.cols, PieceType::I);  // 初期値は未使用だが埋めておく

    // アクティブピース
    constexpr int spawn_col = 3;
    constexpr int spawn_row = 3;
    const int spawn_x = grid.origin_x + spawn_col * grid.cellW;
    const int spawn_y = grid.origin_y + spawn_row * grid.cellH;

    // 7-Bag 初期化と取得
    // registry のコンテキストに PieceQueue を保持（初回のみ emplace）
    auto& pq = registry.ctx().emplace<PieceQueue>();
    if (pq.queue.empty()) {
        refill_bag(pq);
    }
    const PieceType first_type = take_next(pq);

    world.active = registry.create();
    registry.emplace<Position>(world.active, spawn_x, spawn_y);
    registry.emplace<TetriminoMeta>(world.active, first_type, PieceDirection::West,
                                    PieceStatus::Falling);
    registry.emplace<ActivePiece>(world.active);

    const double base_rate = (cfg.dropRate > 0.0) ? (1.0 / cfg.dropRate) : 0.0;
    registry.emplace<Gravity>(world.active, base_rate);
    registry.emplace<FallAccCells>(world.active, 0.0);
    registry.emplace<SoftDrop>(world.active, false, 10.0);

    return world;
}

// 1フレーム更新（純粋システムのスケジューラで実行）
export inline void step_world(const World& w, const Env<GlobalSetting>& env) {
    if (!w.registry) return;
    auto& world = *w.registry;
    if (!world.valid(w.grid_singleton)) return;

    // Resources 構築
    TetrisResources res{env.input, env, w.grid_singleton};

    Schedule<TetrisResources> sch{{
        Phase<TetrisResources>{{make_system<TetrisResources>(inputSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(gravitySystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(resolveRotationSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(resolveLateralSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(hardDropSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(resolveDropSystem_pure)}},
        Phase<TetrisResources>{{
            make_system<TetrisResources>(lockTimerTickSystem_pure)  // ★ 追加
        }},
        Phase<TetrisResources>{{make_system<TetrisResources>(lockAndMergeSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(lineClearSystem_pure)}},
        Phase<TetrisResources>{{
            make_system<TetrisResources>(gameOverCheckSystem_pure)  // ★ 追加：最終チェック
        }},
    }};

    run_schedule(world, res, sch);
}

export bool is_gameover(const World& w) {
    if (!w.registry) return false;
    auto& r = *w.registry;
    if (!r.valid(w.grid_singleton)) return false;
    if (auto* go = r.try_get<GameOver>(w.grid_singleton)) {
        return go->value;
    }
    return false;
}

// 描画（副作用：従来どおり直接描画でOK）
export inline void render_world(const World& world, SDL_Renderer* renderer) {
    if (!world.registry) return;
    auto& registry = *world.registry;

    // 背景
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    // Grid
    if (auto* grid = registry.try_get<GridResource>(world.grid_singleton)) {
        for (int row = 0; row < grid->rows; ++row) {
            for (int col = 0; col < grid->cols; ++col) {
                SDL_Rect rect = grid->rect_rc(row, col);

                // 塗り
                if (grid->occ[grid->index(row, col)] == CellStatus::Filled) {
                    // ★ 追加：設置済みテトリミノの色で描画
                    const PieceType t = grid->occ_type[grid->index(row, col)];
                    const SDL_Color color = to_color(t);
                    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

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
    auto view = registry.view<const ActivePiece, const Position, const TetriminoMeta>();
    for (auto e : view) {
        const auto& pos = view.get<const Position>(e);
        const auto& meta = view.get<const TetriminoMeta>(e);

        const auto* grid = registry.try_get<GridResource>(world.grid_singleton);
        const int cell_width = grid ? grid->cellW : 30;
        const int cell_height = grid ? grid->cellH : 30;

        // 色設定
        const SDL_Color color = to_color(meta.type);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

        // 形状セル
        const auto cells = cells_for(meta.type, meta.direction);
        for (const auto& c : cells) {
            const int x = pos.x + static_cast<int>(c.second) * cell_width;
            const int y = pos.y + static_cast<int>(c.first) * cell_height;
            SDL_Rect rect = {x, y, cell_width, cell_height};
            SDL_RenderFillRect(renderer, &rect);
        }

        // 4x4 グリッド（元 render_grid_around 相当）
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        const int grid_w = cell_width * 4;
        const int grid_h = cell_height * 4;
        SDL_Rect frame = {pos.x, pos.y, grid_w, grid_h};
        SDL_RenderDrawRect(renderer, &frame);
        for (int c = 1; c <= 3; ++c) {
            const int x = pos.x + c * cell_width;
            SDL_RenderDrawLine(renderer, x, pos.y, x, pos.y + grid_h);
        }
        for (int r0 = 1; r0 <= 3; ++r0) {
            const int y = pos.y + r0 * cell_height;
            SDL_RenderDrawLine(renderer, pos.x, y, pos.x + grid_w, y);
        }
    }

    SDL_RenderPresent(renderer);
}

}  // namespace tetris_rule
