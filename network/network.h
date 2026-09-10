#pragma once

#include <cstdint>
#include "World/block_types.h"

enum NetworkChannel {
    CHANNEL_RELIABLE = 0,
    CHANNEL_MOVEMENT = 1,
};

enum class PacketType : uint8_t {
    PlayerPosition       = 1,
    PlayerList           = 2,
    BlockEdit            = 3,
    ChunkData            = 4,
    AssingPlayerId = 5,
    PlayerDisconnected   = 6,
};

constexpr uint16_t PROTOCOL_VERSION = 1;

struct PacketHeader {
    PacketType type;
    uint16_t protocol_version;
};


#pragma pack(push, 1)

struct AssignPlayerIdPacket {
    PacketHeader header;
    uint32_t player_id;
};

struct PlayerDisconnectedPacket {
    PacketHeader header;
    uint32_t player_id;
};

struct PlayerPositionPacket {
    PacketHeader header;
    uint32_t player_id;

    float x;
    float y;
    float z;
};

struct BlockEditPacket {
    PacketHeader header;

    uint32_t player_id;

    int32_t x;
    int32_t y;
    int32_t z;

    BlockType block_type;
};

#pragma pack(pop)