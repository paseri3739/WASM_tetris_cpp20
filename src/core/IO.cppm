module;
#include <SDL2/SDL.h>
#include <functional>
#include <initializer_list>
#include <utility>
export module Io;
export struct Unit {};

export template <class T>
using IO = std::function<T(SDL_Renderer*)>;

export template <class T>
IO<T> pure(T v) {
    return [v = std::move(v)](SDL_Renderer*) -> T { return v; };
}

export inline IO<Unit> sequence(std::initializer_list<IO<Unit>> ios) {
    return [ios](SDL_Renderer* r) -> Unit {
        for (auto& a : ios) a(r);
        return {};
    };
}

export template <class F>
IO<Unit> lift(F&& eff) {
    return [f = std::forward<F>(eff)](SDL_Renderer* r) -> Unit {
        f(r);
        return {};
    };
}
