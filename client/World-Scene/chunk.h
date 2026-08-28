#pragma once

#include "block.h"
#include "Math/noise.h"
#include "biomes.h"

#define CHUNK_SIZE_X 50
#define CHUNK_SIZE_Z 50
#define CHUNK_SIZE_Y 100
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
    std::vector<uint8_t> column_tops;
    // the highest block in each collum (used to be mroe efficient when doing stuff (e.g. generating mesh))

    public:
        Chunk(Noise& continental_noise, Noise& hill_noise, Noise& mountain_noise, Noise& temperature_noise, Noise& moisture_noise, int chunk_x, int chunk_z);
        Chunk();

        std::unique_ptr<Object> object;
        std::unique_ptr<Mesh> mesh;

        MeshData generate_mesh_data();
        inline BlockType get_block(int x, int y, int z) {
            return blocks[x + CHUNK_SIZE_X * (z + CHUNK_SIZE_Z * y)];
        }
        inline void set_block(int x, int y, int z, BlockType block) {
            blocks[x + CHUNK_SIZE_X * (z + CHUNK_SIZE_Z * y)] = block;
            if (block != BlockType::Air && y > column_tops[x + CHUNK_SIZE_X * z]) {
                column_tops[x + CHUNK_SIZE_X * z] = static_cast<uint8_t>(y);
            }
        }
};