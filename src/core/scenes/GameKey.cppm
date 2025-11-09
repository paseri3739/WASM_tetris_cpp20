module;
#include <SDL2/SDL_keycode.h>
#include <optional>
#include <unordered_map>
#include <vector>
export module GameKey;

export namespace game_key {

enum class GameKey {
    LEFT,
    RIGHT,
    UP,
    DOWN,
    ROTATE_LEFT,
    ROTATE_RIGHT,
    DROP,
    PAUSE,
    QUIT,
};

// SDL_Keycode ⇄ GameKey の対応を1か所に定義
inline const std::vector<std::pair<SDL_Keycode, GameKey>> KEY_MAP = {
    {SDLK_a, GameKey::LEFT},       {SDLK_d, GameKey::RIGHT},       {SDLK_w, GameKey::UP},
    {SDLK_s, GameKey::DOWN},       {SDLK_z, GameKey::ROTATE_LEFT}, {SDLK_x, GameKey::ROTATE_RIGHT},
    {SDLK_SPACE, GameKey::DROP},  // ← 追加: ハードドロップ
    {SDLK_RETURN, GameKey::PAUSE}, {SDLK_ESCAPE, GameKey::QUIT}};

// 正方向: SDL_Keycode → GameKey
inline std::optional<GameKey> to_game_key(SDL_Keycode code) {
    for (const auto& [sdl, key] : KEY_MAP) {
        if (sdl == code) return key;
    }
    return std::nullopt;
}

// 逆方向: GameKey → SDL_Keycode（優先される最初のキーを返す）
inline std::optional<SDL_Keycode> to_sdl_key(GameKey game_key) {
    for (const auto& [sdl, key] : KEY_MAP) {
        if (key == game_key) return sdl;
    }
    return std::nullopt;
}

// 逆変換用マップを初期化しておく（複数キー対応が必要なら vector 化）
inline const std::unordered_map<GameKey, SDL_Keycode> PRIMARY_SDL_KEY_FOR_GAMEKEY = [] {
    std::unordered_map<GameKey, SDL_Keycode> map;
    for (const auto& [sdl, key] : KEY_MAP) {
        // まだ登録されていないGameKeyのみ格納（最初のものを代表とする）
        if (!map.contains(key)) {
            map[key] = sdl;
        }
    }
    return map;
}();

}  // namespace game_key
