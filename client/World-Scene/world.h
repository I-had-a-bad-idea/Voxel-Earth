#ifndef WORLD_H
#define WORLD_H

#include <unordered_map>
#include <utility>
#include <algorithm>

#include <VGL/renderer.h>
#include <VGL/object.h>

#include "Math/noise.h"

enum class BlockType {
    Air,
    Stone,
    Dirt,
    Grass,
};

struct AtlasTile { // (0, 0) is top left
    uint32_t x;
    uint32_t y;
};

struct BlockTexture {
    AtlasTile top;
    AtlasTile bottom;
    AtlasTile side;
};

BlockTexture get_block_texture(BlockType type);
constexpr float ATLAS_WIDTH = 16.0f;
constexpr float ATLAS_HEIGHT = 20.0f;

glm::vec2 atlas_uv(AtlasTile, glm::vec2 uv);


#define CHUNK_SIZE_X 32
#define CHUNK_SIZE_Z 32
#define CHUNK_SIZE_Y 128

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
        Chunk(Noise& noise, int chunk_x, int chunk_z);
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

#define RENDER_DISTANCE 5

class World {
    Renderer& renderer;
    Scene scene;

    std::unique_ptr<Mesh> cube_mesh;
    std::unique_ptr<Texture> atlas_texture;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Material> atlas_material;
    
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks;
    
    Noise noise;

    public:
        World(Renderer& renderer_);

        void setup();
        void update(float delta_time);
        void update_chunks();
        Scene& get_scene();

        BlockType get_block(int x, int y, int z);
        void set_block(int x, int y, int z, BlockType);
};


#endif