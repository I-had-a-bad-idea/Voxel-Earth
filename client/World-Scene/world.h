#ifndef WORLD_H
#define WORLD_H

#include <VGL/object.h>
#include <VGL/renderer.h>

#include "Math/noise.h"

enum BlockType {
    BlockType_Air = 0,
    BlockType_Dirt,
    BlockType_Grass,
};

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
    std::unique_ptr<Texture> gravel_texture;
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