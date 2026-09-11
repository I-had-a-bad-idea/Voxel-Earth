#pragma once

#include "block.h"
#include "Math/noise.h"
#include "Math/packing.hpp"
#include "biomes.h"

#define CHUNK_SIZE_X 64
#define CHUNK_SIZE_Z 64
#define CHUNK_SIZE_Y 64
constexpr int SEA_LEVEL = CHUNK_SIZE_Y / 4;

enum class ChunkLOD : char {
    LOD0,
    LOD1,
    LOD2,
    LOD3,
    LOD4,
    LOD5,
    LOD6,
};

struct TerrainColumn {
    int height;
    Biome biome;
    BlockType surface;
};

struct ChunkPos {
    int x;
    int y;
    int z;

    bool operator==(const ChunkPos& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct ChunkPosHash {
    std::size_t operator()(const ChunkPos& pos) const {
        return std::hash<int>()(pos.x) ^
               (std::hash<int>()(pos.y) << 1) ^
               (std::hash<int>()(pos.z) << 2);
    }
};

class Chunk {
    int chunk_x;
    int chunk_y;
    int chunk_z;
    std::vector<BlockType> blocks;
    std::vector<uint8_t> column_tops;
    bool has_blocks = false;
    // the highest block in each collum (used to be mroe efficient when doing stuff (e.g. generating mesh))

    public:
        Chunk(const std::vector<TerrainColumn>& terrain_columns, int chunk_x, int chunk_y, int chunk_z);
        Chunk();

        std::unique_ptr<Object> object;
        std::unique_ptr<Mesh> mesh;
        bool in_scene = false;
        bool dirty = true; // whether the chunk mesh needs to be updated
        ChunkLOD lod = ChunkLOD::LOD0;

        MeshData generate_mesh_data(ChunkLOD requested_lod = ChunkLOD::LOD0);
        static MeshData generate_mesh_data(const std::vector<BlockType>& source_blocks,
                           ChunkLOD requested_lod = ChunkLOD::LOD0);

        std::vector<BlockType> copy_blocks() const;

        inline BlockType get_block(int x, int y, int z) {
            return blocks[x + CHUNK_SIZE_X * (z + CHUNK_SIZE_Z * y)];
        }
        
        inline void set_block(int x, int y, int z, BlockType block) {
            blocks[x + CHUNK_SIZE_X * (z + CHUNK_SIZE_Z * y)] = block;

            if (block != BlockType::Air) {
                has_blocks = true;
            }
            
            int column_index = x + CHUNK_SIZE_X * z;
            
            // If placing a non-Air block higher than current top, update it
            if (block != BlockType::Air && y > column_tops[column_index]) {
                column_tops[column_index] = static_cast<uint8_t>(y);
            }
            // If removing a block that was at the top, recalculate the column top
            else if (block == BlockType::Air && y == column_tops[column_index]) {
                // Find the new highest non-Air block in this column
                int new_top = 0;
                for (int cy = y; cy >= 0; --cy) {
                    if (get_block(x, cy, z) != BlockType::Air) {
                        new_top = cy;
                        break;
                    }
                }
                column_tops[column_index] = static_cast<uint8_t>(new_top);
            }
            
            dirty = true;
            ++mesh_revision;
        }

        uint64_t mesh_revision = 0;
};