#ifndef WORLD_H
#define WORLD_H

#include <VGL/object.h>
#include <VGL/renderer.h>

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

class Block {
    public:
        BlockType block_type;

        Block(BlockType block_type);
        Block();
};


#define CHUNK_SIZE_X 64
#define CHUNK_SIZE_Z 64
#define CHUNK_SIZE_Y 64

class Chunk {
    int chunk_x;
    int chunk_z;
    std::vector<Block> blocks;

    public:
        Chunk(Noise& noise, int chunk_x, int chunk_z);
        Chunk();

        std::unique_ptr<Object> object;
        std::unique_ptr<Mesh> mesh;

        MeshData generate_mesh_data();
        inline Block get_block(int x, int y, int z) {
            return blocks[x + CHUNK_SIZE_X * (z + CHUNK_SIZE_Z * y)];
        }
        inline void set_block(int x, int y, int z, Block block) {
            blocks[x + CHUNK_SIZE_X * (z + CHUNK_SIZE_Z * y)] = block;
        }
};


#define WORLD_SIZE_X 2
#define WORLD_SIZE_Z 2

class World {
    Scene scene;

    std::unique_ptr<Mesh> cube_mesh;
    std::unique_ptr<Texture> atlas_texture;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Material> gravel_material;
    
    std::vector<std::vector<Chunk>> chunks;
    
    Noise noise;

    public:
        World();

        void setup(Renderer& renderer);
        void update(float delta_time);
        Scene& get_scene();
};


#endif