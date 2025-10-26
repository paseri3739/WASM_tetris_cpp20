module;
#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
export module Input;

namespace KeyMapping {
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

/**
 * 入力状態を表現する構造体
 * - key_states: 各キーの状態を保持するマップ
 * - clear_frame_state: フレーム状態をクリアした新インスタンスを返す
 * - to_string: 入力状態を文字列に変換する
 */
export struct Input {
    std::unordered_map<InputKey, InputState> key_states;

    // フレーム状態をクリアした新インスタンスを返す
    [[nodiscard]] std::shared_ptr<const Input> clear_frame_state() const {
        auto next = std::make_shared<Input>(*this);
        for (auto& [_, state] : next->key_states) {
            state.is_pressed = false;
            state.is_released = false;
        }
        return next;
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

        auto maybe_key = KeyMapping::to_input_key(event.key.keysym.sym);
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
}  // namespace KeyMapping
