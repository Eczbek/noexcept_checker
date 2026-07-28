#pragma once

template<typename... Args>
constexpr bool bTHROWS = sizeof...(Args) == 0;

#define THROWS(...) noexcept(::bTHROWS<__VA_ARGS__>)
