#ifndef WORLD_H
#define WORLD_H

#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <algorithm>

#include <VGL/renderer.h>
#include <VGL/object.h>

#include "Math/noise.h"
#include "block.h"
#include "chunk.h"

constexpr int RENDER_DISTANCE = 10;

class World {
    struct GeneratedChunk {
        ChunkPos pos;
        std::unique_ptr<Chunk> chunk;
        MeshData mesh_data;
    };

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

    std::mutex generation_mutex;
    std::condition_variable generation_condition;
    std::queue<ChunkPos> generation_queue;
    std::queue<GeneratedChunk> completed_chunks;
    std::unordered_set<ChunkPos, ChunkPosHash> requested_chunks;
    std::thread generation_thread;
    bool stop_generation {false};

    void generate_chunks();
    void queue_chunk_generation(ChunkPos pos);
    void process_completed_chunks();

    public:
        World(Renderer& renderer_);
        ~World();

        void setup();
        void update(float delta_time);
        void update_chunks();
        Scene& get_scene();

        BlockType get_block(int x, int y, int z);
        void set_block(int x, int y, int z, BlockType);
};


#endif