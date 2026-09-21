#ifndef WORLD_H
#define WORLD_H

#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <algorithm>
#include <vector>

#include <VGL/renderer.h>
#include <VGL/object.h>
#include <VGL/math.h>

#include "real_world_data.h"
#include "Math/noise.h"
#include "block.h"
#include "chunk.h"

constexpr float player_height = 1.0f;
constexpr float player_half_width = 0.3f;


constexpr int MAX_NEW_REQUESTS_PER_FRAME = 16;

constexpr int RENDER_DISTANCE = 35;
constexpr int VERTICAL_RENDER_DISTANCE = 20;
constexpr int UNDERGROUND_STREAM_DISTANCE = 1;
constexpr int ELEVATION_ZOOM = 15;
constexpr int ELEVATION_TILE_CACHE_DISTANCE = 10; // in tiles, not chunks (40 chunks)
// Each tile is 256x256 blocks, each chunk is 64x64x64 blocks, so 1 tile = 4 chunks.

struct PlayerObject {
    uint32_t player_id;
    std::unique_ptr<Object> object;
};

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
        ChunkLOD lod;
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

    struct PendingBlockEdit {
        int x;
        int y;
        int z;
        BlockType block;
    };

    Renderer& renderer;
    Scene scene;

    ElevationTileFetcher elevation_tile_fetcher;
    WorldCoverFetcher world_cover_fetcher;

    std::vector<PlayerObject> player_objects;

    std::unique_ptr<Mesh> player_mesh;
    std::unique_ptr<Texture> atlas_texture;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Material> atlas_material;
    
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks;
    std::unordered_map<ChunkPos, std::vector<PendingBlockEdit>, ChunkPosHash> pending_block_edits;
    

    std::unordered_map<ColumnPos, TerrainColumn, ColumnPosHash> terrain_columns;
    std::unordered_map<ElevationTileCoordinate, ElevationTile, ElevationTileCoordinateHash> elevation_tiles;
    std::unordered_map<WorldCoverTileCoordinate, WorldCoverTile, WorldCoverTileCoordinateHash> world_cover_tiles;

    std::mutex terrain_cache_mutex;

    std::condition_variable elevation_tile_condition;
    std::queue<ElevationTileCoordinate> elevation_tile_queue;

    std::condition_variable world_cover_tile_condition;
    std::queue<WorldCoverTileCoordinate> world_cover_tile_queue;

    std::unordered_set<ElevationTileCoordinate, ElevationTileCoordinateHash> requested_elevation_tiles;
    std::unordered_set<WorldCoverTileCoordinate, WorldCoverTileCoordinateHash> requested_world_cover_tiles;

    std::thread world_data_thread;
    std::thread world_cover_data_thread;
    bool stop_world_data_thread = false;


    std::vector<ChunkPos> new_requests;
    int next_new_request = 0;

    std::mutex generation_mutex;
    std::condition_variable generation_condition;
    std::queue<ChunkPos> generation_queue;
    std::queue<GeneratedChunk> completed_chunks;
    std::unordered_set<ChunkPos, ChunkPosHash> requested_chunks;
    std::atomic<int> generation_camera_chunk_x {0};
    std::atomic<int> generation_camera_chunk_y {0};
    std::atomic<int> generation_camera_chunk_z {0};
    ChunkPos last_stream_camera_chunk {0, 0, 0};
    bool has_stream_camera_chunk {false};
    bool mesh_updates_needed {false};
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
    void prefetch_elevation_tiles(int camera_chunk_x, int camera_chunk_z);
    void fetch_elevation_tiles();
    void fetch_world_cover_tiles();
    TerrainColumn generate_terrain_column(int world_x, int world_z);
    std::vector<TerrainColumn> get_chunk_terrain_columns(int chunk_x, int chunk_z);
    void queue_chunk_generation(ChunkPos pos);
    void process_completed_chunks();
    float get_elevation_height(ElevationTileCoordinate coord, int pixel_x, int pixel_y);
    LandCover get_world_cover(GeoCoordinate geo);

    public:
        World(Renderer& renderer_);
        ~World();

        void setup();
        void update(float delta_time);
        void update_chunks();
        Scene& get_scene();

        BlockType get_block(int x, int y, int z);
        void set_block(int x, int y, int z, BlockType);

        void add_player_object(uint32_t player_id, glm::vec3 position);
        void update_player_object(uint32_t player_id, glm::vec3 position);
        void remove_player_object(uint32_t player_id);
};

MeshData generate_player_mesh();

#endif