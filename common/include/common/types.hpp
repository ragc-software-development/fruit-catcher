#pragma once

#include <cstdint>

namespace ragc::Common {

enum class EntityId : uint32_t
{
    INVALID = 0
};

struct IVec2
{
    int32_t x{0};
    int32_t y{0};

    [[nodiscard]] constexpr bool operator==(const IVec2& other) const noexcept
    {
        return x == other.x && y == other.y;
    }
};

struct Fruit
{
    EntityId id{EntityId::INVALID};
    IVec2 position;
    uint32_t points_value{10};
    bool is_active{false};
};

struct Player
{
    int client_fd{-1};
    IVec2 position;
    uint32_t score{0};
    bool is_connected{false};
    uint64_t last_move_tick{0}; // Add logic to throttle movement speed
};

} // namespace ragc::Common