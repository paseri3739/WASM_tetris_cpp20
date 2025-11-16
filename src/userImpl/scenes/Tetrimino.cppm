module;
#include <SDL2/SDL.h>
#include <array>
#include <entt/entt.hpp>
#include <tl/expected.hpp>
#include <utility>
export module Tetrimino;

// テトリミノ関連enum
export enum class PieceType { I, O, T, S, Z, J, L };
export enum class PieceStatus { Falling, Landed, Merged };
export enum class PieceDirection { North, East, South, West };

// =============================
// 形状ヘルパ(ローカル定義)
// =============================

export using Coord = std::pair<std::int8_t, std::int8_t>;

// 色ユーティリティ
export constexpr SDL_Color to_color(PieceType type) noexcept {
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
/**
 * @brief テトリミノのメタ情報
 * @param type テトリミノ種別
 * @param direction 向き
 * @param status 現在の状態
 * @param rotationCount 現在のY座標で何回回転したか(無限に回転できないようにする)
 */
export struct TetriminoMeta {
    PieceType type{};
    PieceDirection direction{};
    PieceStatus status{};
    int rotationCount{0};
    int minimumY{0};  // ロック/回転リセット用
};

// 依存除去: 引数型を ECS の PieceType / PieceDirection に変更
export constexpr std::array<Coord, 4> get_cells_north_local(PieceType type) noexcept {
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

export constexpr std::array<Coord, 4> get_cells_east_local(PieceType type) noexcept {
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

export constexpr std::array<Coord, 4> get_cells_south_local(PieceType type) noexcept {
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

export constexpr std::array<Coord, 4> get_cells_west_local(PieceType type) noexcept {
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

export constexpr std::array<Coord, 4> cells_for(PieceType type, PieceDirection dir) noexcept {
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
