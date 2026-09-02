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
#include <vector>

#include <VGL/renderer.h>
#include <VGL/object.h>

#include "Math/noise.h"
#include "block.h"
#include "chunk.h"

constexpr int RENDER_DISTANCE = 40;
constexpr int VERTICAL_RENDER_DISTANCE = 5;

class World {
    struct ColumnPos {
        int x;
        int z;

        bool operator==(const ColumnPos& other) const {
            return x == other.x && z == other.z;
        }
    };

    struct ColumnPosHash {
        std::size_t operator()(const ColumnPos& pos) const {
            return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.z) << 1);
        }
    };

    struct GeneratedChunk {
        ChunkPos pos;
        std::unique_ptr<Chunk> chunk;
        MeshData mesh_data;
    };

    struct MeshUpdate {
        ChunkPos pos;
        ChunkLOD lod;
        uint64_t revision;
        std::vector<BlockType> blocks;
    };

    struct CompletedMeshUpdate {
        ChunkPos pos;
        ChunkLOD lod;
        uint64_t revision;
        MeshData mesh_data;
    };

    Renderer& renderer;
    Scene scene;

    std::unique_ptr<Mesh> cube_mesh;
    std::unique_ptr<Texture> atlas_texture;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Material> atlas_material;
    
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks;
    
    Noise continental;
    Noise hills;
    Noise mountains;
    Noise temperature;
    Noise moisture;
    std::unordered_map<ColumnPos, TerrainColumn, ColumnPosHash> terrain_columns;

    std::mutex generation_mutex;
    std::condition_variable generation_condition;
    std::queue<ChunkPos> generation_queue;
    std::queue<GeneratedChunk> completed_chunks;
    std::unordered_set<ChunkPos, ChunkPosHash> requested_chunks;
    std::thread generation_thread;
    bool stop_generation {false};

    std::mutex mesh_update_mutex;
    std::condition_variable mesh_update_condition;
    std::queue<MeshUpdate> mesh_update_queue;
    std::queue<CompletedMeshUpdate> completed_mesh_updates;
    std::unordered_set<ChunkPos, ChunkPosHash> pending_mesh_updates;
    std::thread mesh_update_thread;
    bool stop_mesh_updates {false};

    void generate_chunks();
    void update_chunk_meshes();
    TerrainColumn generate_terrain_column(int world_x, int world_z);
    std::vector<TerrainColumn> get_chunk_terrain_columns(int chunk_x, int chunk_z);
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