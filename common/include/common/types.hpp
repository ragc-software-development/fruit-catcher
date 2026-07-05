#pragma once

#include <cstdint>

namespace ragc::Common {

enum class EntityId : uint32_t
{
    INVALID = 0
};

struct alignas(16) Vec2
{
    float x{0.0f};
    float y{0.0f};

    [[nodiscard]] constexpr float distance_sq(const Vec2& other) const noexcept
    {
        float dx = x - other.x;
        float dy = y - other.y;

        return (dx * dx) + (dy * dy);
    }
};

struct Fruit
{
    EntityId id{EntityId::INVALID};
    Vec2 position;
    uint32_t points_value{10};
    bool is_active{false};
};

struct Player
{
    int client_fd{-1};
    Vec2 position;
    uint32_t score{0};
    bool is_connected{false};
};

} // namespace ragc::Common