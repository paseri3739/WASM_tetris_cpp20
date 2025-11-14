// tests/test_tetris_rule_systems.cpp

#include <gtest/gtest.h>
#include <algorithm>
#include <entt/entt.hpp>
#include <memory>

import TetrisRule;
import GlobalSetting;
import SceneFramework;
import Input;

using global_setting::FontPtr;
using global_setting::GlobalSetting;
using scene_fw::Env;
using tetris_rule::ActivePiece;
using tetris_rule::CellStatus;
using tetris_rule::GridResource;
using tetris_rule::LockTimer;
using tetris_rule::PieceDirection;
using tetris_rule::PieceStatus;
using tetris_rule::PieceType;
using tetris_rule::Position;
using tetris_rule::RotateIntent;
using tetris_rule::TetriminoMeta;
using tetris_rule::World;

// Env を簡単に構築するユーティリティ
static Env<GlobalSetting> makeEnv(const GlobalSetting& setting, const input::Input& input,
                                  double dt) {
    // Env の定義:
    // struct Env {
    //     const input::Input& input;
    //     const Setting& setting;
    //     double dt;
    //     std::function<void(SettingPatch)> queue_setting_update;
    // };
    // なので、この順番で集約初期化します。
    return Env<GlobalSetting>{input, setting, dt, {}};
}

// テスト用 GlobalSetting の生成ユーティリティ
static std::shared_ptr<const GlobalSetting> makeSetting(int columns, int rows, int cellWidth,
                                                        int cellHeight, int fps, double dropRate) {
    // フォントはロジックに不要なので nullptr でよい
    FontPtr font{};  // デフォルト構築で null

    auto s = std::make_shared<GlobalSetting>(columns, rows, cellWidth, cellHeight, fps, dropRate,
                                             std::move(font));

    // shared_ptr<GlobalSetting> → shared_ptr<const GlobalSetting> へ暗黙変換
    return std::shared_ptr<const GlobalSetting>(std::move(s));
}

// ------------------------------------------------------------
// 1. LineClearSystem: 一列埋まっていると消えること
// ------------------------------------------------------------
TEST(TetrisRuleSystems, LineClearRemovesFullBottomRow) {
    constexpr int columns = 4;
    constexpr int rows = 4;
    constexpr int cellW = 16;
    constexpr int cellH = 16;
    constexpr int fps = 60;
    constexpr double dropRate = 0.0;  // 重力を無効化

    auto gs = makeSetting(columns, rows, cellW, cellH, fps, dropRate);
    auto worldExp = tetris_rule::make_world(gs);
    ASSERT_TRUE(worldExp.has_value());
    World w = *worldExp;

    auto& reg = *w.registry;
    auto& grid = reg.get<GridResource>(w.grid_singleton);

    // bottom row をすべて Filled にする
    const int bottom = grid.rows - 1;
    for (int c = 0; c < grid.cols; ++c) {
        const int idx = grid.index(bottom, c);
        grid.occ[idx] = CellStatus::Filled;
    }
    // 変更を registry に書き戻す
    reg.replace<GridResource>(w.grid_singleton, grid);

    // 入力は何もしないダミー
    input::Input input{};
    auto env = makeEnv(*gs, input, 0.0);  // dt=0 なのでロックタイマも進まない

    // 1ステップ実行（LineClearSystem を含む全 System が走る）
    tetris_rule::step_world(w, env);

    // Grid を再取得
    const auto& gridAfter = reg.get<GridResource>(w.grid_singleton);

    // どのセルも Filled ではないこと（行が落ちきって全消去）
    for (int r = 0; r < gridAfter.rows; ++r) {
        for (int c = 0; c < gridAfter.cols; ++c) {
            const int idx = gridAfter.index(r, c);
            EXPECT_EQ(gridAfter.occ[idx], CellStatus::Empty)
                << "row=" << r << " col=" << c << " が Filled のままです";
        }
    }
}

// ------------------------------------------------------------
// 2. GameOverCheckSystem:
//    盤面にブロックが詰まっていて ActivePiece を置けないと gameover になること
// ------------------------------------------------------------
TEST(TetrisRuleSystems, GameOverWhenSpawnOverlapsFilledColumns) {
    constexpr int columns = 10;
    constexpr int rows = 10;
    constexpr int cellW = 16;
    constexpr int cellH = 16;
    constexpr int fps = 60;
    constexpr double dropRate = 0.0;

    auto gs = makeSetting(columns, rows, cellW, cellH, fps, dropRate);
    auto worldExp = tetris_rule::make_world(gs);
    ASSERT_TRUE(worldExp.has_value());
    World w = *worldExp;

    auto& reg = *w.registry;
    auto& grid = reg.get<GridResource>(w.grid_singleton);

    // スポーン位置は make_world 内で
    //   spawn_col = 3; spawn_row = 3;
    // としているので、その 4 列分（3,4,5,6 列）を全行 Filled にする。
    // → 各行は 10 列中 4 列だけ埋まっているので「ライン消去の対象にはならない」が、
    //    ActivePiece の 4x4 形状は必ずこの 4 列のいずれかを使うので配置不能になる。
    for (int r = 0; r < grid.rows; ++r) {
        for (int c = 3; c <= 6; ++c) {
            const int idx = grid.index(r, c);
            grid.occ[idx] = CellStatus::Filled;
        }
    }
    reg.replace<GridResource>(w.grid_singleton, grid);

    input::Input input{};
    auto env = makeEnv(*gs, input, 0.0);

    // 1ステップ実行すると、最後の gameOverCheckSystem_pure で
    // ActivePiece が配置不能と判定され gameover フラグが立つはず
    tetris_rule::step_world(w, env);

    EXPECT_TRUE(tetris_rule::is_gameover(w));
}

// ------------------------------------------------------------
// 3. LockAndMergeSystem + LineClearSystem の一部:
//    Landed + 十分な LockTimer を持つ ActivePiece が Grid に書き込まれ、
//    新しい ActivePiece がスポーンすること（ライン消去と競合しないケース）
// ------------------------------------------------------------
TEST(TetrisRuleSystems, LockAndMergeFixesPieceAndSpawnsNewActive) {
    constexpr int columns = 10;
    constexpr int rows = 20;
    constexpr int cellW = 16;
    constexpr int cellH = 16;
    constexpr int fps = 60;
    constexpr double dropRate = 0.0;  // 重力無効化（位置が変わらないようにする）

    auto gs = makeSetting(columns, rows, cellW, cellH, fps, dropRate);
    auto worldExp = tetris_rule::make_world(gs);
    ASSERT_TRUE(worldExp.has_value());
    World w = *worldExp;

    auto& reg = *w.registry;
    auto& grid = reg.get<GridResource>(w.grid_singleton);

    // ActivePiece を一つ取得
    auto activeView = reg.view<ActivePiece, Position, TetriminoMeta>();
    ASSERT_EQ(activeView.size_hint(), 1u) << "make_world 直後に ActivePiece は 1 つである想定です";
    const entt::entity e = *activeView.begin();

    // 位置を盤面左上近く（0,0セル）に固定
    Position pos{};
    pos.x = grid.origin_x + 0 * grid.cellW;
    pos.y = grid.origin_y + 0 * grid.cellH;
    reg.replace<Position>(e, pos);

    // Meta を「着地済み」の状態にしておく（O ミノ、North で十分）
    TetriminoMeta meta{};
    meta.type = PieceType::O;
    meta.direction = PieceDirection::North;
    meta.status = PieceStatus::Landed;  // 落下完了状態
    reg.replace<TetriminoMeta>(e, meta);

    // 十分大きな LockTimer を付与しておく
    LockTimer lt{};
    lt.sec = 1.0;  // kLockDelaySec=0.3 より大きければ良い
    reg.emplace_or_replace<LockTimer>(e, lt);

    // 全セル Empty にクリアしておく（念のため）
    for (auto& cell : grid.occ) {
        cell = CellStatus::Empty;
    }
    reg.replace<GridResource>(w.grid_singleton, grid);

    input::Input input{};
    auto env = makeEnv(*gs, input, 0.0);  // dt=0: LockTimerTick は sec を増やさない

    // 1ステップ実行
    tetris_rule::step_world(w, env);

    // 1) Grid に固定ブロックが書き込まれているはず（少なくとも 4 セル以上）
    const auto& gridAfter = reg.get<GridResource>(w.grid_singleton);
    const auto filledCount =
        std::count(gridAfter.occ.begin(), gridAfter.occ.end(), CellStatus::Filled);
    EXPECT_GE(filledCount, 4) << "固定されたテトリミノが Grid に反映されていません";

    // 2) ActivePiece は create_then により新規スポーンされているはず
    //    （古い ActivePiece は destroy 済み）
    auto afterView = reg.view<ActivePiece, Position, TetriminoMeta>();
    EXPECT_EQ(afterView.size_hint(), 1u)
        << "ロック後に新しい ActivePiece がちょうど 1 つ存在する想定です";

    // 新しい ActivePiece が Falling 状態であることだけ軽く確認
    const entt::entity newE = *afterView.begin();
    const auto& newMeta = afterView.get<TetriminoMeta>(newE);
    EXPECT_EQ(newMeta.status, PieceStatus::Falling);
}

// ------------------------------------------------------------
// 4. Gravity + ResolveDrop の組み合わせ:
//    dropRate と dt から 1 セル分だけ下に落ちること
// ------------------------------------------------------------
TEST(TetrisRuleSystems, GravityMakesPieceFallOneCellPerSecond) {
    constexpr int columns = 10;
    constexpr int rows = 20;
    constexpr int cellW = 16;
    constexpr int cellH = 16;
    constexpr int fps = 60;
    constexpr double dropRate = 1.0;  // 1.0 [sec / cell] → rate_cps = 1.0 [cell / sec]

    auto gs = makeSetting(columns, rows, cellW, cellH, fps, dropRate);
    auto worldExp = tetris_rule::make_world(gs);
    ASSERT_TRUE(worldExp.has_value());
    World w = *worldExp;

    auto& reg = *w.registry;

    // ActivePiece の現在位置を取得
    auto view = reg.view<ActivePiece, Position, TetriminoMeta>();
    ASSERT_EQ(view.size_hint(), 1u);
    const entt::entity e = *view.begin();
    const auto& posBefore = view.get<Position>(e);

    input::Input input{};
    // dt = 1.0 秒 → 1 セル分の落下が起こるはず
    auto env = makeEnv(*gs, input, 1.0);

    tetris_rule::step_world(w, env);

    // 位置を再取得
    const auto& posAfter = reg.get<Position>(e);

    EXPECT_EQ(posAfter.x, posBefore.x);
    EXPECT_EQ(posAfter.y, posBefore.y + cellH) << "1 秒経過で 1 セル分だけ落下する想定です";
}

// ------------------------------------------------------------
// 5. SRS (Super Rotation System) の検証:
//    Tミノが SRS キックにより「その場では回転できないが、
//    一つ横にずれて回転が成功する」ことを確認する。
//    具体的には North -> East の右回転時、
//    (0,0) では衝突するが (-1,0) キックで成立するケースを作る。
// ------------------------------------------------------------
TEST(TetrisRuleSystems, SRSTSpinKickNorthToEast) {
    constexpr int columns = 10;
    constexpr int rows = 20;
    constexpr int cellW = 16;
    constexpr int cellH = 16;
    constexpr int fps = 60;
    constexpr double dropRate = 0.0;  // 重力無効化

    auto gs = makeSetting(columns, rows, cellW, cellH, fps, dropRate);
    auto worldExp = tetris_rule::make_world(gs);
    ASSERT_TRUE(worldExp.has_value());
    World w = *worldExp;

    auto& reg = *w.registry;
    auto& grid = reg.get<GridResource>(w.grid_singleton);

    // 全セルを一度クリア
    for (auto& cell : grid.occ) {
        cell = CellStatus::Empty;
    }
    for (auto& t : grid.occ_type) {
        t = PieceType::I;
    }

    // ActivePiece（1個）の取得
    auto view = reg.view<ActivePiece, Position, TetriminoMeta>();
    ASSERT_EQ(view.size_hint(), 1u) << "make_world 直後に ActivePiece は 1 つである想定です";
    const entt::entity e = *view.begin();

    // Tミノに強制設定し、向きを North に固定
    TetriminoMeta meta{};
    meta.type = PieceType::T;
    meta.direction = PieceDirection::North;
    meta.status = PieceStatus::Falling;
    reg.replace<TetriminoMeta>(e, meta);

    // 基準位置（4x4 ブロックの左上セル座標）を決める
    // rows, cols とも十分な余白がある (3,4) を採用。
    const int base_row = 3;
    const int base_col = 4;

    Position pos{};
    pos.x = grid.origin_x + base_col * grid.cellW;
    pos.y = grid.origin_y + base_row * grid.cellH;
    reg.replace<Position>(e, pos);

    // その場（North 向き）では衝突しないように盤面を構成しつつ、
    // North -> East の回転時 (0,0) では衝突、(-1,0) のキックでのみ成立する状況を作る。
    //
    // T ミノ North のセル（rr,cc）は:
    //   (0,1), (1,0), (1,1), (1,2)
    // なので、ベースから見て使用行は base_row, base_row+1 のみ。
    //
    // 一方 East のセルは:
    //   (0,1), (1,1), (1,2), (2,1)
    // なので、(2,1) -> (base_row+2, base_col+1) は
    // North では使わないが East では使うセル。
    //
    // ここにブロックを置くことで、
    //   - その場回転 (0,0) は East 配置時に衝突して失敗
    //   - キック (-1,0) で左に 1 セルずらすと、このセルを使わないため成功
    const int block_row = base_row + 2;
    const int block_col = base_col + 1;
    ASSERT_LT(block_row, grid.rows);
    ASSERT_LT(block_col, grid.cols);
    const int block_idx = grid.index(block_row, block_col);
    grid.occ[block_idx] = CellStatus::Filled;
    grid.occ_type[block_idx] = PieceType::O;  // 色は何でもよい
    reg.replace<GridResource>(w.grid_singleton, grid);

    // 事前位置を保存
    const auto posBefore = reg.get<Position>(e);

    // 回転意図を直接付与（右回転 dir=+1）
    RotateIntent ri{};
    ri.dir = +1;
    reg.emplace_or_replace<RotateIntent>(e, ri);

    // 入力は何も押していない状態、dt=0 とする
    input::Input input{};
    auto env = makeEnv(*gs, input, 0.0);

    // 1ステップ実行（SRS 対応 resolveRotationSystem_pure が走る）
    tetris_rule::step_world(w, env);

    // 回転後の情報を取得
    const auto& metaAfter = reg.get<TetriminoMeta>(e);
    const auto& posAfter = reg.get<Position>(e);

    // 向きは East に変わっているはず
    EXPECT_EQ(metaAfter.type, PieceType::T);
    EXPECT_EQ(metaAfter.direction, PieceDirection::East)
        << "Tミノが SRS により North から East へ回転している想定です";

    // 位置は SRS キック (-1, 0) に対応して
    // x 座標が 1セルぶん左へ移動し、y 座標は変わらない想定。
    EXPECT_EQ(posAfter.y, posBefore.y)
        << "North->East の Tスピン SRS では縦方向オフセット dy=0 のキックが選ばれる想定です";
    EXPECT_EQ(posAfter.x, posBefore.x - grid.cellW)
        << "North->East の SRS キック (-1,0) により、1セル左へ移動している想定です";
}
