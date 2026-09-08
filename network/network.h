#pragma once

#include <cstdint>
#include <glm/vec3.hpp>

enum NetworkChannel {
    CHANNEL_RELIABLE = 0,
    CHANNEL_MOVEMENT = 1,
};

enum class PacketType : uint8_t {
    PlayerPosition       = 1,
    PlayerList           = 2,
    BlockEdit            = 3,
    ChunkData            = 4,
    AssingPlayerIdPacket = 5,
};

#pragma pack(push, 1)

struct AssignPlayerIdPacket {
    PacketType type;
    uint32_t player_id;
};

struct PlayerPositionPacket {
    PacketType type;
    uint32_t player_id;

    float x;
    float y;
    float z;
};

struct BlockEditPacket {
    PacketType type;

    int32_t x;
    int32_t y;
    int32_t z;

    uint8_t block_type;
};

#pragma pack(pop)