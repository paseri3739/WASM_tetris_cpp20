module;
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>
#include <entt/entt.hpp>
#include <memory>
#include <optional>
#include <string>
#include <tl/expected.hpp>
#include <vector>

export module TetrisRule;

import GlobalSetting;
import SceneFramework;
import GameKey;
import Input;
import Tetrimino;
import SRS;
import SevenBag;
import Command;
import SDLPtr;

namespace tetris_rule {

// =============================
// エイリアス
// =============================
using global_setting::GlobalSetting;
using scene_fw::Env;

// =============================
// ECS コンポーネント／リソース
// =============================

/**
 * @brief  位置コンポーネント(ピクセル単位)
 * @param x ピクセル単位の X 座標
 * @param y ピクセル単位の Y 座標
 */
export struct Position {
    int x{}, y{};
};

/**
 * @brief 操作対象のテトリミノエンティティ
 */
export struct ActivePiece {
    entt::entity entity{entt::null};
};
/**
 * @brief 重力コンポーネント
 * @param rate_cps 落下速度(セル／秒)
 */
export struct Gravity {
    double rate_cps{};
};
struct FallAccCells {
    double cells{};
};  // cells
struct SoftDrop {
    bool held{false};
    double multiplier{10.0};
};
// ★ 追加：ホールド状態を保持するリソース
export struct HeldPiece {
    std::optional<PieceType> held_type{std::nullopt};
    bool used_in_this_turn{false};  // そのピースで既にホールド済みか
};

/**
 * @brief  ホールドリクエストコンポーネント(押下フレームのみ)
 */
struct HoldRequest {};

/**
 * @brief 移動リクエストコンポーネント
 * @param dx X方向の移動量(セル単位)
 * @param dy Y方向の移動量(セル単位)
 */
struct MoveIntent {
    int dx{0};
    int dy{0};
};

/**
 * @brief 回転リクエストコンポーネント
 * @param dir 回転方向(-1: 左回転, +1: 右回転)
 */
export struct RotateIntent {
    int dir{0};  // -1: 左回転, +1: 右回転
};

/**
 * @brief ロックタイマーコンポーネント
 * @param sec ロックまでの経過時間(秒)
 */
export struct LockTimer {
    double sec{0.0};
};

/**
 * @brief  ハードドロップリクエストコンポーネント(押下フレームのみ)
 */
struct HardDropRequest {
    entt::entity entity{entt::null};
};

// 盤面占有
export enum class CellStatus : std::uint8_t { Empty, Filled };

// ★ 追加：ゲームオーバー状態(Grid のシングルトンにぶら下げる)
struct GameOver {
    bool value{false};
};

// グリッド情報＋占有
export struct GridResource {
    int rows{};
    int cols{};
    int cellW{};
    int cellH{};
    int origin_x{0};
    int origin_y{0};
    // ★ 追加：占有セルのテトリミノ種別(描画色復元用)
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
// ゴースト表示用ヘルパ
// =============================

// Grid 上に (piecePx, piecePy) のピクセル位置で meta のテトリミノを置けるか？
// (ActivePiece 自身は Grid に書き込まれていない前提)
inline bool can_place_on_grid_pixel(const GridResource& grid, const TetriminoMeta& meta,
                                    int piecePx, int piecePy) {
    const auto shape = cells_for(meta.type, meta.direction);

    for (auto [rr, cc] : shape) {
        // ピクセル → グリッド座標変換
        const int col = (piecePx - grid.origin_x) / grid.cellW + cc;
        const int row = (piecePy - grid.origin_y) / grid.cellH + rr;

        // 盤面外なら不可
        if (col < 0 || col >= grid.cols || row < 0 || row >= grid.rows) {
            return false;
        }

        const int idx = grid.index(row, col);
        // 既に固定ブロックがあるなら不可
        if (grid.occ[idx] == CellStatus::Filled) {
            return false;
        }
    }
    return true;
}

// 現在位置から縦方向に落とせるだけ落とした位置(ゴースト位置)を返す
inline Position compute_ghost_position(const GridResource& grid, const Position& currentPos,
                                       const TetriminoMeta& meta) {
    Position ghost = currentPos;
    const int step_py = grid.cellH;  // 1セルぶんのピクセル

    // 次の 1 セル下にまだ置けるあいだ落とし続ける
    while (can_place_on_grid_pixel(grid, meta, ghost.x, ghost.y + step_py)) {
        ghost.y += step_py;
    }
    return ghost;
}

/**
 * @brief ワールドハンドル(シーンから利用)
 * @param registry ECS レジストリ
 * @param grid_singleton グリッドリソースエンティティ
 * @param active 操作中ピースエンティティ
 */
export struct World {
    std::shared_ptr<entt::registry> registry;
    entt::entity grid_singleton{entt::null};
    entt::entity active{entt::null};
};

/**
 * @brief 共通リソース
 * @param input 入力
 * @param env 環境情報
 * @param grid_e グリッドリソースエンティティ
 */
struct TetrisResources {
    const input::Input& input;
    const Env<GlobalSetting>& env;
    entt::entity grid_e{entt::null};
};

// =============================
// Systems(純粋版)
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

/**
 * @brief
 * 入力処理システム(純粋)移動／回転／ソフトドロップ／ハードドロップ要求を処理して積み、後続システムで解決
 *
 * @param ro 読み取りコンポーネント
 * @param wr 書き込みコマンド
 * @param res リソース
 * @return CommandList コマンドリスト
 */
static CommandList inputSystem_pure(
    ReadOnlyView<ActivePiece, SoftDrop, MoveIntent, RotateIntent, HardDropRequest> ro,
    WriteCommands<SoftDrop, MoveIntent, RotateIntent, HardDropRequest, HoldRequest> wr,
    const TetrisResources& res) {
    CommandList out;
    const auto down_key = game_key::to_sdl_key(game_key::GameKey::DOWN);
    auto v = ro.view<ActivePiece, SoftDrop>();

    auto moveView = ro.view<MoveIntent>();
    auto rotView = ro.view<RotateIntent>();

    for (auto e : v) {
        // SoftDrop held 更新(読み取り専用ビュー → “置換コマンド”)
        {
            const auto& sd = v.template get<SoftDrop>(e);
            SoftDrop next = sd;
            next.held = (down_key && res.input.held(*down_key));
            out.emplace_back(wr.emplace_or_replace<SoftDrop>(e, next));
        }

        // 左右／回転は MoveIntent / RotateIntent を押下瞬間のみ積む(元コメント維持)
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

        // --- ハードドロップ要求(押下瞬間のみ) ---
        if (const auto drop = game_key::to_sdl_key(game_key::GameKey::DROP);
            drop && res.input.pressed(*drop)) {
            out.emplace_back(wr.emplace_or_replace<HardDropRequest>(e));
        }

        // --- 追記: ホールド要求(押下瞬間のみ) ---
        if (const auto hold = game_key::to_sdl_key(game_key::GameKey::HOLD);
            hold && res.input.pressed(*hold)) {
            out.emplace_back(wr.emplace_or_replace<HoldRequest>(e));
        }
    }
    return out;
}

// =============================
// ホールド解決(純粋)
// =============================
static CommandList resolveHoldSystem_pure(
    ReadOnlyView<GridResource, ActivePiece, Position, TetriminoMeta, HoldRequest> ro,
    WriteCommands<void> wr, const TetrisResources& res) {
    CommandList out;
    if (!ro.valid(res.grid_e)) return out;

    const auto& grid = ro.get<GridResource>(res.grid_e);
    const int spawn_x = grid.origin_x + res.env.setting.spawn_col * grid.cellW;
    const int spawn_y = grid.origin_y + res.env.setting.spawn_row * grid.cellH;

    // ★ ここから先は Command 側でエンティティを探す
    out.emplace_back(Command{[grid_e = res.grid_e, spawn_x, spawn_y](entt::registry& r) {
        // ActivePiece + HoldRequest を持つエンティティを再検索
        auto v = r.view<ActivePiece, Position, TetriminoMeta, HoldRequest>();
        if (v.begin() == v.end()) {
            return;  // HOLD する対象がいなければ終了
        }
        const entt::entity target = *v.begin();

        auto& grid = r.get<GridResource>(grid_e);
        auto& held = r.ctx().get<HeldPiece>();
        auto& pq = r.ctx().get<PieceQueue>();

        // 既にこのピースでホールド済みなら、リクエストだけ消す TODO: 取り出す
        if (held.used_in_this_turn) {
            if (r.valid(target) && r.any_of<HoldRequest>(target)) {
                r.remove<HoldRequest>(target);
            }
            return;
        }
        held.used_in_this_turn = true;

        // --- 防御的チェック: target / コンポーネントの存在を確認 ---
        if (!r.valid(target) || !r.all_of<TetriminoMeta, Position>(target)) {
            // 想定外の状態なので、HoldRequest だけ掃除して終了
            if (r.valid(target) && r.any_of<HoldRequest>(target)) {
                r.remove<HoldRequest>(target);
            }
            return;
        }

        // ここから先は元のロジックそのまま
        auto& meta = r.get<TetriminoMeta>(target);
        auto& pos = r.get<Position>(target);

        PieceType new_type;

        if (!held.held_type.has_value()) {
            // 初回ホールド: 現在の種を保存し、Bag から新規取得
            held.held_type = meta.type;

            if (pq.queue.empty()) {
                refill_bag(pq);
            }
            new_type = take_next(pq);
        } else {
            // 2回目以降: ホールドと交換
            new_type = *held.held_type;
            held.held_type = meta.type;
        }

        meta.type = new_type;
        meta.direction = PieceDirection::West;
        meta.status = PieceStatus::Falling;
        meta.rotationCount = 0;
        meta.minimumY = spawn_y;

        pos.x = spawn_x;
        pos.y = spawn_y;

        if (r.any_of<LockTimer>(target)) {
            r.remove<LockTimer>(target);
        }
        if (auto* acc = r.try_get<FallAccCells>(target)) {
            acc->cells = 0.0;
        }
        if (auto* sd = r.try_get<SoftDrop>(target)) {
            sd->held = false;
        }

        if (r.any_of<HoldRequest>(target)) {
            r.remove<HoldRequest>(target);
        }
    }});

    return out;
}

/**
 * @brief 重力(純粋)：FallAccCells と MoveIntent.dy を更新
 *
 * @param ro 読み取り
 * @param wr 書き込み
 * @param res リソース
 * @return CommandList コマンドリスト
 */
static CommandList gravitySystem_pure(
    ReadOnlyView<ActivePiece, Gravity, FallAccCells, SoftDrop, MoveIntent> ro,
    WriteCommands<FallAccCells, MoveIntent> wr, const TetrisResources& res) {
    CommandList out;
    auto v = ro.view<ActivePiece, Gravity, FallAccCells, SoftDrop>();
    auto moveView = ro.view<MoveIntent>();

    for (auto e : v) {
        const auto& g = v.template get<Gravity>(e);
        const auto& sd = v.template get<SoftDrop>(e);

        FallAccCells acc = v.template get<FallAccCells>(e);
        const double rate = g.rate_cps * (sd.held ? sd.multiplier : 1.0);
        acc.cells += res.env.dt * rate;

        int steps = static_cast<int>(std::floor(acc.cells));
        steps = std::max(0, std::min(steps, res.env.setting.maxDropsPerFrame));

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
// 回転解決(純粋)
// --- 追記: 回転解決(クラシック／壁蹴りなし) -> SRS 対応に変更 ---
// =============================
static CommandList resolveRotationSystem_pure(
    ReadOnlyView<GridResource, ActivePiece, Position, TetriminoMeta, RotateIntent, LockTimer> ro,
    WriteCommands<RotateIntent, TetriminoMeta, LockTimer, Position> wr,
    const TetrisResources& res) {
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

        const int kMaxRotationLocks = res.env.setting.maxRotationLocks;

        // ★ 追加:
        //    着地中かつ回転回数が上限に達している場合は、
        //    これ以上回転もロックリセットもしない
        if (meta.status == PieceStatus::Landed && meta.rotationCount >= kMaxRotationLocks) {
            continue;
        }

        // SRS 用: 回転後方向
        const PieceDirection ndir = rotate_next(meta.direction, (ri.dir > 0 ? +1 : -1));

        // O ミノも一応方向だけは変えるが、形状は同一なので見た目は変わらない
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

        // ★ ここが SRS 本体：キックテーブルを順に試す
        const auto kicks = srs_kicks(meta.type, meta.direction, ndir);

        bool rotated = false;
        int applied_px = pos.x;
        int applied_py = pos.y;

        for (const auto& k : kicks) {
            const int test_px = pos.x + k.dx * grid->cellW;
            const int test_py = pos.y + k.dy * grid->cellH;
            if (can_place(test_px, test_py)) {
                rotated = true;
                applied_px = test_px;
                applied_py = test_py;
                break;
            }
        }

        if (!rotated) {
            // どのキックでも置けない場合は不採用(何もしない)
            continue;
        }

        // 回転確定
        PieceStatus prevStatus = meta.status;  // ★ 追加：ステータス変化を検知するために保存
        meta.direction = ndir;

        // ★ 回転成功時 (prevStatus が Falling でも増やす)
        meta.rotationCount++;

        // 回転成功時はロック解除(ただし15回まで)
        if (meta.status != PieceStatus::Falling && meta.rotationCount <= kMaxRotationLocks) {
            meta.status = PieceStatus::Falling;
        }

        // 位置とメタ情報を反映、LockTimer 解除
        out.emplace_back(wr.emplace_or_replace<TetriminoMeta>(e, meta));
        out.emplace_back(wr.emplace_or_replace<Position>(e, applied_px, applied_py));

        // ★ 修正ポイント:
        //    「着地(Landed) → 落下(Falling) に変わったときだけ」LockTimer をリセットする
        if (prevStatus == PieceStatus::Landed && meta.status == PieceStatus::Falling &&
            meta.rotationCount <= kMaxRotationLocks) {
            out.emplace_back(wr.remove<LockTimer>(e));
        }
    }
    return out;
}

// =============================
// 横移動解決(純粋)
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
        // 水平方向ぶんはここで消費(垂直は resolveDrop へ委譲)
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
                // 壁/ブロックに当たる場合はそれ以上進めない(残りは破棄)
                break;
            }
            pos.x = nx;
        }
        out.emplace_back(wr.emplace_or_replace<Position>(e, pos));

        // 横移動ではロック状態は変更しない(縦落下系に委譲)
    }
    return out;
}

// =============================
// ロックタイマ加算(純粋)：着地中は毎フレーム dt を加算
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
// 縦落下解決(純粋)
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
            if (pos.y > meta.minimumY) {
                meta.minimumY = pos.y;
                meta.rotationCount = 0;  // 最低到達点更新で回転カウントリセット
            }
        }

        out.emplace_back(wr.emplace_or_replace<Position>(e, pos));
        out.emplace_back(wr.emplace_or_replace<TetriminoMeta>(e, meta));
    }
    return out;
}

// =============================
// ハードドロップ(純粋)
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

        // 設置：即ロック扱い(次フレームで確実に Merge)
        meta.status = PieceStatus::Landed;
        LockTimer lt{};
        lt.sec = res.env.setting.lockDelaySec;

        out.emplace_back(wr.emplace_or_replace<Position>(e, pos));
        out.emplace_back(wr.emplace_or_replace<TetriminoMeta>(e, meta));
        out.emplace_back(wr.emplace_or_replace<LockTimer>(e, lt));
        out.emplace_back(wr.remove<HardDropRequest>(e));  // 消費
    }
    return out;
}

// =============================
// ロック & マージ(純粋)
// =============================
static CommandList lockAndMergeSystem_pure(
    ReadOnlyView<GridResource, ActivePiece, Position, TetriminoMeta, LockTimer> ro,
    WriteCommands<GridResource> wr, const TetrisResources& res) {
    CommandList out;
    if (!ro.valid(res.grid_e)) return out;
    auto grid = ro.get<GridResource>(res.grid_e);  // 書換え用コピー(最後に置換コマンドで反映)

    std::vector<entt::entity> to_fix;
    auto v = ro.view<ActivePiece, Position, TetriminoMeta, LockTimer>();
    for (auto e : v) {
        const auto& meta = v.template get<TetriminoMeta>(e);
        const auto& lt = v.template get<LockTimer>(e);
        if (meta.status == PieceStatus::Falling) continue;
        if (lt.sec < res.env.setting.lockDelaySec) continue;
        to_fix.push_back(e);
    }

    for (auto e : to_fix) {
        const auto& pos = v.template get<Position>(e);
        const auto& meta = v.template get<TetriminoMeta>(e);

        std::array<Coord, 4> cells = cells_for(meta.type, meta.direction);
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

        // 新規スポーン(7-Bag)
        out.emplace_back(wr.create_then([&](entt::registry& r, entt::entity ne) {
            auto& g = r.get<GridResource>(res.grid_e);
            const int spawn_x = g.origin_x + res.env.setting.spawn_col * g.cellW;
            const int spawn_y = g.origin_y + res.env.setting.spawn_row * g.cellH;

            // ★ 追加：新しいピース出現時にホールド使用フラグをリセット
            if (auto* held = r.ctx().find<HeldPiece>()) {
                held->used_in_this_turn = false;
            }

            // registry のコンテキストに保存してある PieceQueue を使用
            auto& piece_queue = r.ctx().get<PieceQueue>();
            if (piece_queue.queue.empty()) refill_bag(piece_queue);
            const PieceType next_type = take_next(piece_queue);

            r.emplace<Position>(ne, spawn_x, spawn_y);
            r.emplace<TetriminoMeta>(ne, next_type, PieceDirection::West, PieceStatus::Falling, 0,
                                     spawn_y);
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
// ライン消去(純粋)
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
            // ★ 任意：既定値でクリア(未使用だが保守性のため)
            grid.occ_type[idx] = PieceType::I;
        }
    }

    out.emplace_back(wr.emplace_or_replace<GridResource>(res.grid_e, grid));
    (void)res;
    return out;
}

// =============================
// ゲームオーバー判定(純粋)
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
            // 以降のループは不要(単一アクティブ前提)。複数あっても一つで判定十分。
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
    const Env<global_setting::GlobalSetting>& env) {
    World world{};
    world.registry = std::make_shared<entt::registry>();
    auto& registry = *world.registry;
    const auto& cfg = env.setting;

    // GridResource(singleton 的エンティティ)
    world.grid_singleton = registry.create();
    auto& grid = registry.emplace<GridResource>(world.grid_singleton);
    // ★ 追加：ゲームオーバーフラグ初期化
    registry.emplace<GameOver>(world.grid_singleton, GameOver{false});
    grid.rows = cfg.gridRows;
    grid.cols = cfg.gridColumns;
    grid.cellW = cfg.cellWidth;
    grid.cellH = cfg.cellHeight;
    grid.origin_x = cfg.holdAreaWidth;  // 左側にホールドエリア分のオフセット
    grid.origin_y = 0;
    grid.occ.assign(grid.rows * grid.cols, CellStatus::Empty);
    grid.occ_type.assign(grid.rows * grid.cols, PieceType::I);  // 初期値は未使用だが埋めておく

    // アクティブピース
    const int spawn_x = grid.origin_x + cfg.spawn_col * grid.cellW;
    const int spawn_y = grid.origin_y + cfg.spawn_row * grid.cellH;

    // 7-Bag 初期化と取得
    // registry のコンテキストに PieceQueue を保持(初回のみ emplace)
    auto& pq = registry.ctx().emplace<PieceQueue>();
    // ★ 追加：ホールド情報もコンテキストに保持
    auto& held = registry.ctx().emplace<HeldPiece>();
    held.used_in_this_turn = false;
    if (pq.queue.empty()) {
        refill_bag(pq);
    }
    const PieceType first_type = take_next(pq);

    world.active = registry.create();
    registry.emplace<Position>(world.active, spawn_x, spawn_y);
    registry.emplace<TetriminoMeta>(world.active, first_type, PieceDirection::West,
                                    PieceStatus::Falling, 0, spawn_y);
    registry.emplace<ActivePiece>(world.active);

    const double base_rate = (cfg.dropRate > 0.0) ? (1.0 / cfg.dropRate) : 0.0;
    registry.emplace<Gravity>(world.active, base_rate);
    registry.emplace<FallAccCells>(world.active, 0.0);
    registry.emplace<SoftDrop>(world.active, false, 10.0);

    return world;
}

// 1フレーム更新(純粋システムのスケジューラで実行)
export inline void step_world(const World& w, const Env<GlobalSetting>& env) {
    if (!w.registry) return;
    auto& world = *w.registry;
    if (!world.valid(w.grid_singleton)) return;

    // Resources 構築
    TetrisResources res{env.input, env, w.grid_singleton};

    Schedule<TetrisResources> sch{{
        Phase<TetrisResources>{{make_system<TetrisResources>(inputSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(resolveHoldSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(gravitySystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(resolveRotationSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(resolveLateralSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(hardDropSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(resolveDropSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(lockTimerTickSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(lockAndMergeSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(lineClearSystem_pure)}},
        Phase<TetrisResources>{{make_system<TetrisResources>(gameOverCheckSystem_pure)}},
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

inline void render_grid(const World& world, SDL_Renderer* const renderer) {
    auto& registry = *world.registry;
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
}

inline void render_tetrimino_cells(SDL_Renderer* const renderer, int originX, int originY,
                                   const PieceType& type, const PieceDirection& direction,
                                   int cellW, int cellH, SDL_Color color) {
    // 色設定
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    // 形状セル
    const auto cells = cells_for(type, direction);
    for (const auto& c : cells) {
        const int x = originX + static_cast<int>(c.second) * cellW;
        const int y = originY + static_cast<int>(c.first) * cellH;
        SDL_Rect rect = {x, y, cellW, cellH};
        SDL_RenderFillRect(renderer, &rect);
    }
}

inline void render_current_tetrimino(const World& world, SDL_Renderer* const renderer) {
    auto& registry = *world.registry;
    // ActivePiece 描画(TetriMino の描画を流用 -> ECS ローカルで置換)
    auto view = registry.view<const ActivePiece, const Position, const TetriminoMeta>();
    for (auto e : view) {
        const auto& pos = view.get<const Position>(e);
        const auto& meta = view.get<const TetriminoMeta>(e);

        const auto* grid = registry.try_get<GridResource>(world.grid_singleton);
        const int cell_width = grid ? grid->cellW : 30;
        const int cell_height = grid ? grid->cellH : 30;

        // ★ 追加：ゴースト描画(落下予定位置のシルエット)
        if (grid) {
            const Position ghostPos = compute_ghost_position(*grid, pos, meta);

            // ゴースト用の色(同じ色でアルファのみ薄くする)
            SDL_Color ghostColor = to_color(meta.type);
            ghostColor.a = 80;

            render_tetrimino_cells(renderer, ghostPos.x, ghostPos.y, meta.type, meta.direction,
                                   cell_width, cell_height, ghostColor);
        }

        // 色設定(本体)
        SDL_Color color = to_color(meta.type);
        // color.a は to_color 側のデフォルトをそのまま利用
        // 必要ならここで color.a を上書き

        // 形状セル(本体)
        render_tetrimino_cells(renderer, pos.x, pos.y, meta.type, meta.direction, cell_width,
                               cell_height, color);

        // 4x4 グリッド(元 render_grid_around 相当)
        // SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        // const int grid_w = cell_width * 4;
        // const int grid_h = cell_height * 4;
        // SDL_Rect frame = {pos.x, pos.y, grid_w, grid_h};
        // SDL_RenderDrawRect(renderer, &frame);
        // for (int c = 1; c <= 3; ++c) {
        //     const int x = pos.x + c * cell_width;
        //     SDL_RenderDrawLine(renderer, x, pos.y, x, pos.y + grid_h);
        // }
        // for (int r0 = 1; r0 <= 3; ++r0) {
        //     const int y = pos.y + r0 * cell_height;
        //     SDL_RenderDrawLine(renderer, pos.x, y, pos.x + grid_w, y);
        // }
    }
}

export inline void render_next_area(const World& world, SDL_Renderer* const renderer,
                                    const Env<GlobalSetting>& env) {
    auto& registry = *world.registry;
    const auto& setting = env.setting;

    const int cellW = setting.cellWidth;
    const int cellH = setting.cellHeight;
    const int gridW = setting.gridAreaWidth;
    const int holdW = setting.holdAreaWidth;

    const int marginX = 10;
    const int marginY = 10;
    const int baseX = holdW + gridW + +marginX;
    int baseY = marginY;

    // "NEXT" ラベル描画
    if (TTF_Font* font = setting.get_font()) {
        const char* label = "NEXT";
        SDL_Color color = {0, 0, 0, 255};

        SurfacePtr surface(TTF_RenderUTF8_Blended(font, label, color), SDL_FreeSurface);
        if (surface) {
            TexturePtr texture(SDL_CreateTextureFromSurface(renderer, surface.get()),
                               SDL_DestroyTexture);
            if (texture) {
                SDL_Rect dstRect{baseX, baseY, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture.get(), nullptr, &dstRect);
            }
        }

        baseY += cellH * 2;
    }

    // ★ ここを ctx からの取得に変更する
    // registry.ctx() に PieceQueue が存在しない場合は何も描画しないで終了
    auto* pq = registry.ctx().find<PieceQueue>();
    if (!pq) {
        return;
    }

    const auto queue_view = view_queue(*pq);

    // 先読みで表示する最大個数（必要に応じて変更）
    constexpr std::size_t max_preview = 5;
    std::size_t index = 0;

    for (PieceType piece_type : queue_view) {
        if (index >= max_preview) break;

        const int panelX = baseX;
        const int panelY =
            baseY + static_cast<int>(index) * (cellH * 4 + cellH);  // 1 個分の高さ + 余白

        // パネル枠（4x4）
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 128);
        SDL_Rect frame{panelX, panelY, cellW * 4, cellH * 4};
        SDL_RenderDrawRect(renderer, &frame);

        // グリッド線（任意）
        for (int c = 1; c <= 3; ++c) {
            const int x = panelX + c * cellW;
            SDL_RenderDrawLine(renderer, x, panelY, x, panelY + cellH * 4);
        }
        for (int r = 1; r <= 3; ++r) {
            const int y = panelY + r * cellH;
            SDL_RenderDrawLine(renderer, panelX, y, panelX + cellW * 4, y);
        }

        // テトリミノ本体（NEXT は固定向きとする）
        const SDL_Color color = to_color(piece_type);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

        const auto cells = cells_for(piece_type, PieceDirection::West);
        for (const auto& c : cells) {
            const int x = panelX + static_cast<int>(c.second) * cellW;
            const int y = panelY + static_cast<int>(c.first) * cellH;
            SDL_Rect rect{x, y, cellW, cellH};
            SDL_RenderFillRect(renderer, &rect);
        }

        ++index;
    }
}

export inline void render_hold_area(const World& world, SDL_Renderer* const renderer,
                                    const Env<GlobalSetting>& env) {
    auto& registry = *world.registry;
    const auto& setting = env.setting;

    const int cellW = setting.cellWidth;
    const int cellH = setting.cellHeight;
    const int holdW = setting.holdAreaWidth;

    const int marginX = 10;
    const int marginY = 10;
    const int baseX = marginX;
    int baseY = marginY;

    // "HOLD" ラベル描画
    if (TTF_Font* font = setting.get_font()) {
        const char* label = "HOLD";
        SDL_Color color = {0, 0, 0, 255};

        SurfacePtr surface(TTF_RenderUTF8_Blended(font, label, color), SDL_FreeSurface);
        if (surface) {
            TexturePtr texture(SDL_CreateTextureFromSurface(renderer, surface.get()),
                               SDL_DestroyTexture);
            if (texture) {
                SDL_Rect dstRect{baseX, baseY, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture.get(), nullptr, &dstRect);
            }
        }

        baseY += cellH * 2;
    }

    // ★ 追加: HOLD パネル枠と中身の描画
    const int panelX = baseX;
    const int panelY = baseY;

    // 枠(4x4)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 128);
    SDL_Rect frame{panelX, panelY, cellW * 4, cellH * 4};
    SDL_RenderDrawRect(renderer, &frame);

    // グリッド線（任意）
    for (int c = 1; c <= 3; ++c) {
        const int x = panelX + c * cellW;
        SDL_RenderDrawLine(renderer, x, panelY, x, panelY + cellH * 4);
    }
    for (int r = 1; r <= 3; ++r) {
        const int y = panelY + r * cellH;
        SDL_RenderDrawLine(renderer, panelX, y, panelX + cellW * 4, y);
    }

    // コンテキストに HeldPiece が無ければここで終了
    auto* held = registry.ctx().find<HeldPiece>();
    if (!held || !held->held_type.has_value()) {
        return;  // 枠だけ表示
    }

    // ホールド中のテトリミノを描画
    const PieceType piece_type = *held->held_type;
    SDL_Color color = to_color(piece_type);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    const auto cells = cells_for(piece_type, PieceDirection::West);
    for (const auto& c : cells) {
        const int x = panelX + static_cast<int>(c.second) * cellW;
        const int y = panelY + static_cast<int>(c.first) * cellH;
        SDL_Rect rect{x, y, cellW, cellH};
        SDL_RenderFillRect(renderer, &rect);
    }
}

// 描画(副作用：従来どおり直接描画でOK)
export inline void render_world(const World& world, SDL_Renderer* const renderer,
                                const Env<GlobalSetting>& env) {
    if (!world.registry) return;

    // アルファブレンド有効化(ゴースト半透明描画用)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 背景クリア
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    // グリッド描画
    render_grid(world, renderer);

    // ActivePiece 描画(TetriMino の描画を流用 -> ECS ローカルで置換)
    render_current_tetrimino(world, renderer);
    // TODO: NEXT / HOLD 表示
    render_next_area(world, renderer, env);
    render_hold_area(world, renderer, env);
    SDL_RenderPresent(renderer);
}

}  // namespace tetris_rule
