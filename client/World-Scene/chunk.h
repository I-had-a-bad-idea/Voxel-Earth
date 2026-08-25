#pragma once

#include "block.h"
#include "Math/noise.h"
#include "biomes.h"

#define CHUNK_SIZE_X 32
#define CHUNK_SIZE_Z 32
#define CHUNK_SIZE_Y 128
constexpr int SEA_LEVEL = CHUNK_SIZE_Y / 4;

struct ChunkPos {
    int x;
    int z;

    bool operator==(const ChunkPos& other) const {
        return x == other.x && z == other.z;
    }
};

struct ChunkPosHash {
    std::size_t operator()(const ChunkPos& pos) const {
        return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.z) << 1);
    }
};

class Chunk {
    int chunk_x;
    int chunk_z;
    std::vector<BlockType> blocks;

    public:
        Chunk(Noise& height_noise, Noise& detail_noise, Noise& temperature_noise, Noise& moisture_noise, int chunk_x, int chunk_z);
        Chunk();

        std::unique_ptr<Object> object;
        std::unique_ptr<Mesh> mesh;

        MeshData generate_mesh_data();
        inline BlockType get_block(int x, int y, int z) {
            return blocks[x + CHUNK_SIZE_X * (z + CHUNK_SIZE_Z * y)];
        }
        inline void set_block(int x, int y, int z, BlockType block) {
            blocks[x + CHUNK_SIZE_X * (z + CHUNK_SIZE_Z * y)] = block;
        }
};