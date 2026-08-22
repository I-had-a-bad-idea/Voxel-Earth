#ifndef WORLD_H
#define WORLD_H

#include <VGL/object.h>
#include <VGL/renderer.h>

#include "Math/noise.h"

enum BlockType {
    BlockType_Default = 0,
    BlockType_Dirt,
    BlockType_Grass,
};

class Block {
    public:
        BlockType block_type;

        Block(BlockType block_type);
        Block();
};


#define CHUNK_SIZE_X 10
#define CHUNK_SIZE_Z 10
#define CHUNK_SIZE_Y 10

class Chunk {
    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Z][CHUNK_SIZE_Y];
    int chunk_x;
    int chunk_z;

    public:
        Chunk(Noise& noise, int chunk_x, int chunk_z);
        Chunk();

        std::unique_ptr<Object> object;
        std::unique_ptr<Mesh> mesh;

        MeshData generate_mesh_data();
};


#define WORLD_SIZE_X 1
#define WORLD_SIZE_Z 1

class World {
    Scene scene;

    std::unique_ptr<Mesh> cube_mesh;
    std::unique_ptr<Texture> gravel_texture;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Material> gravel_material;
    
    Chunk chunks[WORLD_SIZE_X][WORLD_SIZE_Z];
    
    Noise noise;

    public:
        World();

        void setup(Renderer& renderer);
        void update(float delta_time);
        Scene& get_scene();
};


#endif