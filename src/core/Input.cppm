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
 */
export enum class InputKey { UP, DOWN, LEFT, RIGHT, ROTATE_LEFT, ROTATE_RIGHT, DROP, PAUSE, QUIT };

/**
 * SDL キーコードと InputKey のマッピング
 * SDL のキーコードを InputKey に変換するためのマッピングを定義します。
 * これにより、SDL イベントからゲームの入力キーに変換できます。
 */
inline const std::pair<SDL_Keycode, InputKey> SDL_TO_INPUT_KEY_MAP[] = {
    // 移動
    {SDLK_LEFT, InputKey::LEFT},
    {SDLK_a, InputKey::LEFT},
    {SDLK_RIGHT, InputKey::RIGHT},
    {SDLK_d, InputKey::RIGHT},
    {SDLK_UP, InputKey::UP},
    {SDLK_w, InputKey::UP},
    {SDLK_DOWN, InputKey::DOWN},
    {SDLK_s, InputKey::DOWN},
    // 回転
    {SDLK_z, InputKey::ROTATE_LEFT},
    {SDLK_LCTRL, InputKey::ROTATE_LEFT},
    {SDLK_x, InputKey::ROTATE_RIGHT},
    {SDLK_UP, InputKey::ROTATE_RIGHT},  // 二重割り当て（W/↑で回転も許容する場合）
    // ハードドロップ
    {SDLK_SPACE, InputKey::DROP},
    // ポーズ
    {SDLK_p, InputKey::PAUSE},
    {SDLK_RETURN, InputKey::PAUSE},  // Enterでもポーズ許容
    // 終了
    {SDLK_ESCAPE, InputKey::QUIT},
    {SDLK_q, InputKey::QUIT},
};

/**
 * SDL キーコードを InputKey に変換する関数
 * @param code SDL_Keycode
 * @return std::optional<InputKey> 変換結果（存在しない場合は std::nullopt）
 */
inline std::optional<InputKey> to_input_key(SDL_Keycode code) {
    for (const auto& [sdl, input] : SDL_TO_INPUT_KEY_MAP) {
        if (sdl == code) return input;
    }
    return std::nullopt;
}

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

// enum class を unordered_map のキーに使うためのハッシュ特化
}  // namespace input

namespace std {
template <>
struct hash<input::InputKey> {
    size_t operator()(const input::InputKey& k) const noexcept {
        using U = std::underlying_type_t<input::InputKey>;
        return std::hash<U>{}(static_cast<U>(k));
    }
};
}  // namespace std

namespace input {
/**
 * 入力状態を表現する構造体
 * - key_states: 各キーの状態を保持するマップ
 * - clear_frame_state: フレーム状態をクリアした新インスタンスを返す
 * - to_string: 入力状態を文字列に変換する
 *
 * 追加API（呼び出し側での利用を簡便化）:
 * - pressed/released/held: 指定キーの瞬間/保持状態の取得
 * - any_pressed/any_released/any_held: いずれかのキーが該当するか
 * - first_pressed/first_released/first_held: 優先順に最初に該当したキー
 * - get_input_key: 全体の集約状態（いずれかが立っていれば OR 集約を返す）
 */
export struct Input {
    std::unordered_map<InputKey, InputState> key_states;

    // 判定優先順（UI/ゲーム側での期待に応じて調整可能）
    static constexpr InputKey PRIORITY_ORDER[] = {
        InputKey::UP,    InputKey::DOWN,        InputKey::LEFT,
        InputKey::RIGHT, InputKey::ROTATE_LEFT, InputKey::ROTATE_RIGHT,
        InputKey::DROP,  InputKey::PAUSE,       InputKey::QUIT};

    // フレーム状態をクリアした新インスタンスを返す
    [[nodiscard]] std::shared_ptr<const Input> clear_frame_state() const {
        auto next = std::make_shared<Input>(*this);
        for (auto& [_, state] : next->key_states) {
            state.is_pressed = false;
            state.is_released = false;
        }
        return next;
    }

    // --- 低レベル: 単一キーの状態取得 ---
    [[nodiscard]] bool pressed(InputKey k) const {
        if (auto it = key_states.find(k); it != key_states.end()) return it->second.is_pressed;
        return false;
    }
    [[nodiscard]] bool released(InputKey k) const {
        if (auto it = key_states.find(k); it != key_states.end()) return it->second.is_released;
        return false;
    }
    [[nodiscard]] bool held(InputKey k) const {
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
    [[nodiscard]] std::optional<InputKey> first_pressed() const {
        for (auto k : PRIORITY_ORDER)
            if (pressed(k)) return k;
        return std::nullopt;
    }
    [[nodiscard]] std::optional<InputKey> first_released() const {
        for (auto k : PRIORITY_ORDER)
            if (released(k)) return k;
        return std::nullopt;
    }
    [[nodiscard]] std::optional<InputKey> first_held() const {
        for (auto k : PRIORITY_ORDER)
            if (held(k)) return k;
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

/**
 * 抽象入力を取得する関数
 * @param previous_input 以前の入力状態(エッジ検出のために使用)
 * @return 新しい入力状態
 */
export std::shared_ptr<const Input> poll_input(std::shared_ptr<const Input> previous_input) {
    // 初回フレームなど、previous_input が null の場合に備える
    if (!previous_input) {
        previous_input = std::make_shared<const Input>();
    }

    auto input = std::make_shared<Input>(*previous_input);  // コピーして操作対象にする
    for (auto& [_, state] : input->key_states) {
        state.is_pressed = false;
        state.is_released = false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            input->key_states[InputKey::QUIT].is_pressed = true;
            continue;
        }

        if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) continue;

        auto maybe_key = input::to_input_key(event.key.keysym.sym);
        if (!maybe_key.has_value()) continue;

        InputKey key = maybe_key.value();
        InputState& state = input->key_states[key];

        if (event.type == SDL_KEYDOWN) {
            if (!state.is_held) state.is_pressed = true;
            state.is_held = true;
        } else {
            state.is_held = false;
            state.is_released = true;
        }
    }

    return input;
}
}  // namespace input
