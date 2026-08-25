#ifndef WORLD_H
#define WORLD_H

#include <unordered_map>
#include <utility>
#include <algorithm>

#include <VGL/renderer.h>
#include <VGL/object.h>

#include "Math/noise.h"
#include "block.h"
#include "chunk.h"

#define RENDER_DISTANCE 5

class World {
    Renderer& renderer;
    Scene scene;

    std::unique_ptr<Mesh> cube_mesh;
    std::unique_ptr<Texture> atlas_texture;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Material> atlas_material;
    
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks;
    
    Noise height_noise;
    Noise detail_noise;
    Noise temperature_noise;
    Noise moisture_noise;

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