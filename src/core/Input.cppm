module;
#include <SDL2/SDL.h>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
export module Input;

namespace input {

/**
 * 抽象キー入力を表現するenum
 *
 * ※ 本モジュールでは SDL のキーコードをそのまま扱うため、
 *    この enum は利用側で独自に定義して SDL_Keycode とマッピングしてください。
 *    （例: GameInputKey など）
 */
// export enum class InputKey { UP, DOWN, LEFT, RIGHT, ROTATE_LEFT, ROTATE_RIGHT, DROP, PAUSE, QUIT
// };

/**
 * SDL キーコードと InputKey のマッピング
 * SDL のキーコードを InputKey に変換するためのマッピングを定義します。
 * これにより、SDL イベントからゲームの入力キーに変換できます。
 *
 * ※ 本モジュールでは固定マッピングを提供せず、
 *    SDL_Keycode をそのまま上位へ渡し、呼び出し側で任意のマッピングを行ってください。
 */
// inline const std::pair<SDL_Keycode, InputKey> SDL_TO_INPUT_KEY_MAP[] = { ... };

/**
 * SDL キーコードを InputKey に変換する関数
 * @param code SDL_Keycode
 * @return std::optional<InputKey> 変換結果（存在しない場合は std::nullopt）
 *
 * ※ 本モジュールでは変換を行いません。
 *    呼び出し側で SDL_Keycode -> 独自の enum への変換関数を定義してください。
 */
// export inline std::optional<InputKey> to_input_key(SDL_Keycode code) { ... }

/**
 * キー入力の状態を表現する構造体
 * - is_pressed: キーが押された瞬間
 * - is_released: キーが離された瞬間
 * - is_held: キーが押され続けている状態
 */
export struct InputState {
    bool is_pressed = false;
    bool is_released = false;
    bool is_held = false;
};

}  // namespace input

namespace input {

/**
 * 入力状態を表現する構造体
 * - key_states: 各キー(SDL_Keycode)の状態を保持するマップ
 * - clear_frame_state: フレーム状態をクリアした新インスタンスを返す
 * - to_string: 入力状態を文字列に変換する
 *
 * 追加API（呼び出し側での利用を簡便化）:
 * - pressed/released/held: 指定キーの瞬間/保持状態の取得
 * - any_pressed/any_released/any_held: いずれかのキーが該当するか
 * - first_pressed/first_released/first_held: 優先順に最初に該当したキー
 * - get_input_key: 全体の集約状態（いずれかが立っていれば OR 集約を返す）
 *
 * ※ 優先順や抽象キーは、本モジュールでは固定しません。
 *    必要に応じて呼び出し側で SDL_Keycode の配列・enum を定義してご利用ください。
 */
export struct Input {
    // SDL_Keycode をそのままキーとして扱う
    std::unordered_map<SDL_Keycode, InputState> key_states;

    // フレーム状態をクリアした新インスタンスを返す
    [[nodiscard]] std::shared_ptr<const Input> clear_frame_state() const {
        auto next = std::make_shared<Input>(*this);
        for (auto& [_, state] : next->key_states) {
            state.is_pressed = false;
            state.is_released = false;
        }
        return next;
    }

    // --- 低レベル: 単一キー(SDL_Keycode)の状態取得 ---
    [[nodiscard]] bool pressed(SDL_Keycode k) const {
        if (auto it = key_states.find(k); it != key_states.end()) return it->second.is_pressed;
        return false;
    }
    [[nodiscard]] bool released(SDL_Keycode k) const {
        if (auto it = key_states.find(k); it != key_states.end()) return it->second.is_released;
        return false;
    }
    [[nodiscard]] bool held(SDL_Keycode k) const {
        if (auto it = key_states.find(k); it != key_states.end()) return it->second.is_held;
        return false;
    }

    // --- 中レベル: 全体/集合の簡易判定 ---
    [[nodiscard]] bool any_pressed() const {
        for (const auto& [_, s] : key_states)
            if (s.is_pressed) return true;
        return false;
    }
    [[nodiscard]] bool any_released() const {
        for (const auto& [_, s] : key_states)
            if (s.is_released) return true;
        return false;
    }
    [[nodiscard]] bool any_held() const {
        for (const auto& [_, s] : key_states)
            if (s.is_held) return true;
        return false;
    }

    // --- 高レベル: 優先順で最初に該当したキーを返す ---
    // 呼び出し側が優先順コンテナを用意して渡す形にします。
    template <typename It>
    [[nodiscard]] std::optional<SDL_Keycode> first_pressed(It begin, It end) const {
        for (auto it = begin; it != end; ++it)
            if (pressed(*it)) return *it;
        return std::nullopt;
    }
    template <typename It>
    [[nodiscard]] std::optional<SDL_Keycode> first_released(It begin, It end) const {
        for (auto it = begin; it != end; ++it)
            if (released(*it)) return *it;
        return std::nullopt;
    }
    template <typename It>
    [[nodiscard]] std::optional<SDL_Keycode> first_held(It begin, It end) const {
        for (auto it = begin; it != end; ++it)
            if (held(*it)) return *it;
        return std::nullopt;
    }

    /**
     * 全キーの論理和を返す集約ビュー。
     * いずれかのキーで is_pressed/is_released/is_held が立っていれば、それぞれ true にして返す。
     * 全て false の場合は std::nullopt。
     */
    [[nodiscard]]
    std::optional<InputState> get_input_key() const {
        InputState acc{};
        for (const auto& [_, s] : key_states) {
            acc.is_pressed = acc.is_pressed || s.is_pressed;
            acc.is_released = acc.is_released || s.is_released;
            acc.is_held = acc.is_held || s.is_held;
        }
        if (acc.is_pressed || acc.is_released || acc.is_held) return acc;
        return std::nullopt;
    }

    [[nodiscard]]
    std::string to_string() const {
        std::string result;
        for (const auto& [key, state] : key_states) {
            result += "Key: " + std::to_string(static_cast<int>(key)) +
                      ", Pressed: " + std::to_string(state.is_pressed) +
                      ", Released: " + std::to_string(state.is_released) +
                      ", Held: " + std::to_string(state.is_held) + "\n";
        }
        return result;
    }
};

}  // namespace input
