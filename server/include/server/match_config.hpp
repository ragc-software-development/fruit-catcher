#pragma once

#include <concepts>
#include <cstddef>

namespace ragc::Server {

template <size_t MaxPlayers, size_t MaxFruits>
struct MatchConfig
{
    static constexpr size_t max_players = MaxPlayers;
    static constexpr size_t max_fruits = MaxFruits;

    static constexpr float collision_radius_sq = 1.0f * 1.0f;
    static constexpr float map_bounds = 15.0f;
};

template <typename T>
concept ValidMatchConfig = requires {
    { T::max_players } -> std::convertible_to<size_t>;
    { T::max_fruits } -> std::convertible_to<size_t>;
    requires T::max_players > 0 && T::max_players <= 16;
};

} // namespace ragc::Server