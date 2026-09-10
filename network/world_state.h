#pragma once

#include <cstddef>
#include <unordered_map>

#include "World/block_types.h"

struct WorldBlockPosition {
    int x;
    int y;
    int z;

    bool operator==(const WorldBlockPosition& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct WorldBlockPositionHash {
    std::size_t operator()(const WorldBlockPosition& position) const {
        const std::size_t x_hash = std::hash<int>{}(position.x);
        const std::size_t y_hash = std::hash<int>{}(position.y);
        const std::size_t z_hash = std::hash<int>{}(position.z);
        return x_hash ^ (y_hash << 1) ^ (z_hash << 2);
    }
};

class ServerWorldState {
    std::unordered_map<WorldBlockPosition, BlockType, WorldBlockPositionHash> blocks;

public:
    BlockType get_block(int x, int y, int z) const;
    void set_block(int x, int y, int z, BlockType block);
};
