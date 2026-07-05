#pragma once

#include "common/types.hpp"

#pragma pack(push, 1)

namespace ragc::common::Network {

enum class OpCode : uint8_t
{
    CLIENT_INPUT = 1,
    SERVER_MATCH_STATE = 2
};

// 1. Packet [Client -> Sever] (Move input)
struct ClientInputPacket
{
    OpCode op_code{OpCode::CLIENT_INPUT};
    Vec2 direction;
};

struct NetworkPlayerState
{
    int32_t id;
    Vec2 position;
    uint32_t score;
    bool is_active;
};

struct NetworkFruitState
{
    uint32_t id;
    Vec2 position;
    bool is_active;
};

// 2. Packet [Server -> Client] (Broadcast world state every tick)
struct ServerMatchStatePacket
{
    OpCode op_code{OpCode::SERVER_MATCH_STATE};
    uint64_t tick_number;

    NetworkPlayerState players[2];
    NetworkFruitState fruits[50];
};

} // namespace ragc::common::Network

#pragma pack(pop)
