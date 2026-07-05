#pragma once

#include <cstddef>
#include "common/types.hpp"

namespace ragc::Common::Network {

#pragma pack(push, 1)

enum class OpCode : uint8_t
{
    CLIENT_INPUT = 1,
    SERVER_MATCH_STATE = 2,
    CLIENT_JOIN_REQUEST = 3,
    SERVER_JOIN_RESPONSE = 4
};

// 1. Packet [Client -> Sever] (Move input)
struct ClientInputPacket
{
    OpCode op_code{OpCode::CLIENT_INPUT};
    Vec2 direction;
};

// Handshake Packet [Client -> Server] (Request to join a match)
struct ClientJoinRequestPacket
{
    OpCode op_code{OpCode::CLIENT_JOIN_REQUEST};
    char player_name[32]{};
};

// Handshake Packet [Server -> Client] (Confirm match joined & supply limits)
struct ServerJoinResponsePacket
{
    OpCode op_code{OpCode::SERVER_JOIN_RESPONSE};
    int32_t assigned_client_id;
    uint32_t max_players;
    uint32_t max_fruits;
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
template <size_t MaxPlayers, size_t MaxFruits>
struct ServerMatchStatePacket
{
    OpCode op_code{OpCode::SERVER_MATCH_STATE};
    uint64_t tick_number;

    NetworkPlayerState players[MaxPlayers];
    NetworkFruitState fruits[MaxFruits];
};

#pragma pack(pop)

} // namespace ragc::Common::Network
