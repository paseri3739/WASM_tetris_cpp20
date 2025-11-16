module;
#include <SDL2/SDL.h>
#include <memory>
export module SDLPtr;
export using SurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
export using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
