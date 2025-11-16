module;
#include <array>
export module SRS;

import Tetrimino;

// =============================
// SRS キックオフセット定義
// =============================

export struct KickOffset {
    int dx;  // 列方向オフセット(セル単位)
    int dy;  // 行方向オフセット(セル単位)
};

export constexpr int dir_index(PieceDirection d) noexcept {
    switch (d) {
        case PieceDirection::North:
            return 0;
        case PieceDirection::East:
            return 1;
        case PieceDirection::South:
            return 2;
        case PieceDirection::West:
            return 3;
    }
    return 0;
}

// JLSTZ 用 SRS キックテーブル(回転 0, R, 2, L = North, East, South, West)
// 参照: Tetris Guideline SRS
export constexpr std::array<KickOffset, 5> srs_kicks_jlstz(PieceDirection from,
                                                           PieceDirection to) noexcept {
    using D = PieceDirection;
    // 0 -> R
    if (from == D::North && to == D::East) {
        return {{{0, 0}, {-1, 0}, {-1, +1}, {0, -2}, {-1, -2}}};
    }
    // R -> 0
    if (from == D::East && to == D::North) {
        return {{{0, 0}, {+1, 0}, {+1, -1}, {0, +2}, {+1, +2}}};
    }
    // R -> 2
    if (from == D::East && to == D::South) {
        return {{{0, 0}, {+1, 0}, {+1, -1}, {0, +2}, {+1, +2}}};
    }
    // 2 -> R
    if (from == D::South && to == D::East) {
        return {{{0, 0}, {-1, 0}, {-1, +1}, {0, -2}, {-1, -2}}};
    }
    // 2 -> L
    if (from == D::South && to == D::West) {
        return {{{0, 0}, {+1, 0}, {+1, +1}, {0, -2}, {+1, -2}}};
    }
    // L -> 2
    if (from == D::West && to == D::South) {
        return {{{0, 0}, {-1, 0}, {-1, -1}, {0, +2}, {-1, +2}}};
    }
    // L -> 0
    if (from == D::West && to == D::North) {
        return {{{0, 0}, {+1, 0}, {+1, +1}, {0, -2}, {+1, -2}}};
    }
    // 0 -> L
    if (from == D::North && to == D::West) {
        return {{{0, 0}, {-1, 0}, {-1, -1}, {0, +2}, {-1, +2}}};
    }

    // その他(使わないが保険として全部 0)
    return {{{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}};
}

// I ミノ用 SRS キックテーブル
export constexpr std::array<KickOffset, 5> srs_kicks_i(PieceDirection from,
                                                       PieceDirection to) noexcept {
    using D = PieceDirection;
    // 0 -> R
    if (from == D::North && to == D::East) {
        return {{{0, 0}, {-2, 0}, {+1, 0}, {-2, -1}, {+1, +2}}};
    }
    // R -> 0
    if (from == D::East && to == D::North) {
        return {{{0, 0}, {+2, 0}, {-1, 0}, {+2, +1}, {-1, -2}}};
    }
    // R -> 2
    if (from == D::East && to == D::South) {
        return {{{0, 0}, {-1, 0}, {+2, 0}, {-1, +2}, {+2, -1}}};
    }
    // 2 -> R
    if (from == D::South && to == D::East) {
        return {{{0, 0}, {+1, 0}, {-2, 0}, {+1, -2}, {-2, +1}}};
    }
    // 2 -> L
    if (from == D::South && to == D::West) {
        return {{{0, 0}, {+2, 0}, {-1, 0}, {+2, +1}, {-1, -2}}};
    }
    // L -> 2
    if (from == D::West && to == D::South) {
        return {{{0, 0}, {-2, 0}, {+1, 0}, {-2, -1}, {+1, +2}}};
    }
    // L -> 0
    if (from == D::West && to == D::North) {
        return {{{0, 0}, {+1, 0}, {-2, 0}, {+1, -2}, {-2, +1}}};
    }
    // 0 -> L
    if (from == D::North && to == D::West) {
        return {{{0, 0}, {-1, 0}, {+2, 0}, {-1, +2}, {+2, -1}}};
    }

    return {{{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}};
}

export constexpr std::array<KickOffset, 5> srs_kicks(PieceType type, PieceDirection from,
                                                     PieceDirection to) noexcept {
    if (type == PieceType::I) {
        return srs_kicks_i(from, to);
    }
    if (type == PieceType::O) {
        // O ミノは SRS 上はオフセット 0 のみ(実質見た目変化なし)
        return {{{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}};
    }
    // J, L, S, T, Z
    return srs_kicks_jlstz(from, to);
}
