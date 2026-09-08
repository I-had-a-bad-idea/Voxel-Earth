#include "World.h"

namespace {
ChunkLOD lod_for_chunk_distance(int dx, int dy, int dz) {
    const int distance = std::max(std::abs(dx), std::max(std::abs(dy), std::abs(dz)));
    if (distance <= 2) {
        return ChunkLOD::LOD0;
    }
    if (distance <= 4) {
        return ChunkLOD::LOD1;
    }
    if (distance <= 8) {
        return ChunkLOD::LOD2;
    }
    if (distance <= 12) {
        return ChunkLOD::LOD3;
    }
    if (distance <= 16) {
        return ChunkLOD::LOD4;
    }
    if (distance <= 20) {
        return ChunkLOD::LOD5;
    }
    if (distance <= 25) {
        return ChunkLOD::LOD10;
    }
    if (distance <= 32) {
        return ChunkLOD::LOD32;
    } 
    return ChunkLOD::LOD64;
}
}

World::World(Renderer& renderer_)
    : renderer(renderer_),
      continental(1234, 0.0008f, 4, 2.0f, 0.5f, FastNoiseLite::FractalType_FBm),
      hills(5678, 0.006f, 4, 2.0f, 0.5f, FastNoiseLite::FractalType_FBm),
      mountains(9012, 0.0025f, 5, 2.1f, 0.55f, FastNoiseLite::FractalType_Ridged),
      temperature(3456, 0.0015f, 3, 2.0f, 0.5f, FastNoiseLite::FractalType_FBm),
      moisture(7890, 0.0015f, 3, 2.0f, 0.5f, FastNoiseLite::FractalType_FBm)
{
    generation_thread = std::thread(&World::generate_chunks, this);
    mesh_update_thread = std::thread(&World::update_chunk_meshes, this);
}

World::~World() {
    {
        std::lock_guard lock(generation_mutex);
        stop_generation = true;
    }
    generation_condition.notify_one();
    generation_thread.join();

    {
        std::lock_guard lock(mesh_update_mutex);
        stop_mesh_updates = true;
    }
    mesh_update_condition.notify_one();
    mesh_update_thread.join();
}

void World::generate_chunks() {
    while (true) {
        ChunkPos pos;
        {
            std::unique_lock lock(generation_mutex);
            generation_condition.wait(lock, [this] { // wait for work or end
                return stop_generation || !generation_queue.empty();
            });

            if (stop_generation && generation_queue.empty()) {
                return;
            }

            // get work
            pos = generation_queue.front();
            generation_queue.pop();
        }

        // Noise depends only on horizontal position, so reuse it across vertical chunks.
        std::vector<TerrainColumn> terrain = get_chunk_terrain_columns(pos.x, pos.z);
        auto chunk = std::make_unique<Chunk>(
            terrain,
            pos.x,
            pos.y,
            pos.z
        );
        const ChunkLOD lod = lod_for_chunk_distance(
            pos.x - generation_camera_chunk_x.load(std::memory_order_relaxed),
            pos.y - generation_camera_chunk_y.load(std::memory_order_relaxed),
            pos.z - generation_camera_chunk_z.load(std::memory_order_relaxed)
        );
        MeshData mesh_data = chunk->generate_mesh_data(lod);

        {
            std::lock_guard lock(generation_mutex);
            completed_chunks.push({pos, std::move(chunk), lod, std::move(mesh_data)}); // submit as completed
        }
    }
}

void World::update_chunk_meshes() {
    while (true) {
        MeshUpdate update;
        {
            std::unique_lock lock(mesh_update_mutex);
            mesh_update_condition.wait(lock, [this] {
                return stop_mesh_updates || !mesh_update_queue.empty();
            });

            if (stop_mesh_updates && mesh_update_queue.empty()) {
                return;
            }

            update = std::move(mesh_update_queue.front());
            mesh_update_queue.pop();
        }

        MeshData mesh_data = Chunk::generate_mesh_data(update.blocks, update.lod);
        {
            std::lock_guard lock(mesh_update_mutex);
            completed_mesh_updates.push({
                update.pos,
                update.lod,
                update.revision,
                std::move(mesh_data)
            });
        }
    }
}

ElevationTile& World::get_elevation_tile(int zoom, TileCoordinate coord) {
    if (elevation_tiles.contains(coord)) {
        return elevation_tiles.at(coord);
    }
    ElevationTile tile_data = elevation_tile_fetcher.elevation_tile_fetch(zoom, coord.x, coord.y);
    auto [it, inserted] = elevation_tiles.emplace(coord, std::move(tile_data));

    return it->second;
}


TerrainColumn World::generate_terrain_column(int world_x, int world_z) {
    const GeoCoordinate geo = world_to_geo(world_x, world_z);
    const TileCoordinate tile = geo_to_tile(geo, ELEVATION_ZOOM);
    
    // Get noise + convert -1..1 -> 0..1
    float temperature_value = (temperature.at(static_cast<float>(world_x), static_cast<float>(world_z)) + 1.0f) * 0.5f;
    float moisture_value = (moisture.at(static_cast<float>(world_x), static_cast<float>(world_z)) + 1.0f) * 0.5f;

    const ElevationTile& elevation = get_elevation_tile(ELEVATION_ZOOM, tile);
    // Find pixel in tile corresponding to world coordinates
    int pixel_x = world_x % ElevationTile::SIZE;
    int pixel_y = world_z % ElevationTile::SIZE;
    if (pixel_x < 0) pixel_x += ElevationTile::SIZE;
    if (pixel_y < 0) pixel_y += ElevationTile::SIZE;
    float height_f = elevation.get(pixel_x, pixel_y);


    TerrainColumn column;
    column.height = static_cast<int>(std::round(height_f / METERS_PER_WORLD_BLOCK));
    // if (mountain_factor > 0.45f) {
    //     column.biome = Biome::Mountains;
    // }
    if (temperature_value < 0.30f) {
        column.biome = Biome::Tundra;
    } else if (temperature_value > 0.70f && moisture_value < 0.35f) {
        column.biome = Biome::Desert;
    } else if (moisture_value > 0.65f) {
        column.biome = Biome::Forest;
    } else {
        column.biome = Biome::Plains;
    }

    column.surface = BlockType::Grass;
    if (column.height <= SEA_LEVEL + 2 || column.biome == Biome::Desert) {
        column.surface = BlockType::Sand;
    } else if (column.biome == Biome::Tundra || (column.biome == Biome::Mountains && column.height > CHUNK_SIZE_Y * 0.72f)) {
        column.surface = BlockType::Snow;
    } else if (column.biome == Biome::Mountains) {
        column.surface = BlockType::Stone;
    }
    return column;
}

std::vector<TerrainColumn> World::get_chunk_terrain_columns(int chunk_x, int chunk_z) {
    std::vector<TerrainColumn> columns(CHUNK_SIZE_X * CHUNK_SIZE_Z);
    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            const ColumnPos pos{chunk_x * CHUNK_SIZE_X + x, chunk_z * CHUNK_SIZE_Z + z};
            auto [it, inserted] = terrain_columns.try_emplace(pos);
            if (inserted) {
                it->second = generate_terrain_column(pos.x, pos.z);
            }
            columns[x + CHUNK_SIZE_X * z] = it->second;
        }
    }
    return columns;
}

void World::queue_chunk_generation(ChunkPos pos) {
    if (chunks.contains(pos) || requested_chunks.contains(pos)) {
        return;
    }

    // add work
    requested_chunks.insert(pos);
    {
        std::lock_guard lock(generation_mutex);
        generation_queue.push(pos);
    }
    generation_condition.notify_one(); // wake up thread
}

void World::process_completed_chunks() { // on main thread
    GeneratedChunk generated;
    std::size_t uploaded_chunks = 0;

    // Keep generation from building an unbounded queue while allowing the
    // initial visible area to stream in at several chunks per frame.
    while (uploaded_chunks < 8) {
        {
            std::lock_guard lock(generation_mutex);
            if (completed_chunks.empty()) {
                return;
            }

            // get chunk
            generated = std::move(completed_chunks.front());
            completed_chunks.pop();
        }

        requested_chunks.erase(generated.pos);
        if (chunks.contains(generated.pos)) {
            continue;
        }


        // add to chunk list
        auto [it, inserted] = chunks.emplace(generated.pos, std::move(*generated.chunk));
        if (!inserted) {
            continue;
        }

        // create mesh and object
        Chunk& chunk = it->second;
        const ChunkLOD desired_lod = lod_for_chunk_distance(
            generated.pos.x - static_cast<int>(std::floor(scene.cam_pos.x / CHUNK_SIZE_X)),
            generated.pos.y - static_cast<int>(std::floor(scene.cam_pos.y / CHUNK_SIZE_Y)),
            generated.pos.z - static_cast<int>(std::floor(scene.cam_pos.z / CHUNK_SIZE_Z))
        );
        chunk.lod = desired_lod;
        if (generated.lod != desired_lod) {
            generated.mesh_data = chunk.generate_mesh_data(desired_lod);
        }
        bool empty_chunk = generated.mesh_data.vertices.empty() || generated.mesh_data.indices.empty();
        if (empty_chunk) {
            chunk.dirty = false;
            continue;
        }


        chunk.mesh = std::make_unique<Mesh>(renderer.load_mesh(std::move(generated.mesh_data)));

        chunk.dirty = false;
        chunk.object = std::make_unique<Object>(
            chunk.mesh.get(),
            atlas_material.get(),
            glm::vec3(generated.pos.x * CHUNK_SIZE_X, generated.pos.y * CHUNK_SIZE_Y, generated.pos.z * CHUNK_SIZE_Z),
            glm::vec3(0.0f, 0.0f, 0.0f)
        );
        if (!empty_chunk) {
            scene.add_object_to_scene(chunk.object.get()); // add to world
        }
        ++uploaded_chunks;
    }
}

void World::update_chunks() {
    glm::mat4 camera_transform = glm::translate(
        glm::mat4(1.0f),
        scene.cam_pos
    ) * glm::mat4_cast(scene.cam_orientation);
    glm::vec3 camera_forward = glm::normalize(glm::vec3(
        camera_transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)
    ));

    int camera_chunk_x = static_cast<int>(std::floor(scene.cam_pos.x / CHUNK_SIZE_X));
    int camera_chunk_y = static_cast<int>(std::floor(scene.cam_pos.y / CHUNK_SIZE_Y));
    int camera_chunk_z = static_cast<int>(std::floor(scene.cam_pos.z / CHUNK_SIZE_Z));

    generation_camera_chunk_x.store(camera_chunk_x, std::memory_order_relaxed);
    generation_camera_chunk_y.store(camera_chunk_y, std::memory_order_relaxed);
    generation_camera_chunk_z.store(camera_chunk_z, std::memory_order_relaxed);

    queue_chunk_generation({camera_chunk_x, 0, camera_chunk_z});
    for (int y = camera_chunk_y - VERTICAL_RENDER_DISTANCE; y <= camera_chunk_y + VERTICAL_RENDER_DISTANCE; ++y) {
        queue_chunk_generation({camera_chunk_x, y, camera_chunk_z});
    }

    for (int step = 1; step <= RENDER_DISTANCE; ++step) {
        int min_x = camera_chunk_x - step;
        int max_x = camera_chunk_x + step;
        int min_z = camera_chunk_z - step;
        int max_z = camera_chunk_z + step;

        auto queue_vertical_range = [&](int x, int z) {
            const int dx = x - camera_chunk_x;
            const int dz = z - camera_chunk_z;
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) {
                return;
            }

            queue_chunk_generation({x, 0, z});
            int start_y = camera_chunk_y - VERTICAL_RENDER_DISTANCE;
            if (step > UNDERGROUND_STREAM_DISTANCE) {
                start_y = std::max(0, start_y); // dont generate chunks underground
            }

            for (int y = start_y; y <= camera_chunk_y + VERTICAL_RENDER_DISTANCE; ++y) {
                queue_chunk_generation({x, y, z});
            }
        };

        // Bottom row
        for (int x = min_x; x <= max_x; ++x) {
            queue_vertical_range(x, min_z);
        }

        // Top row
        for (int x = min_x; x <= max_x; ++x) {
            queue_vertical_range(x, max_z);
        }

        // Left and right columns, excluding corners
        for (int z = min_z + 1; z < max_z; ++z) {
            queue_vertical_range(min_x, z);
            queue_vertical_range(max_x, z);
        }
    }

    process_completed_chunks();


    // Remove chunks that are too far away
    std::vector<ChunkPos> chunks_to_remove;
    for (const auto& [pos, chunk] : chunks) {
        int dx = pos.x - camera_chunk_x;
        int dy = pos.y - camera_chunk_y;
        int dz = pos.z - camera_chunk_z;
        const bool underground_too_far = pos.y < 0 &&
            (std::abs(dx) > UNDERGROUND_STREAM_DISTANCE ||
             std::abs(dz) > UNDERGROUND_STREAM_DISTANCE);
        if (std::abs(dx) > RENDER_DISTANCE ||
            std::abs(dy) > VERTICAL_RENDER_DISTANCE ||
            std::abs(dz) > RENDER_DISTANCE ||
            underground_too_far) {
            chunks_to_remove.push_back(pos);
        }
    }
    for (const ChunkPos& pos : chunks_to_remove) {
        {
            std::lock_guard lock(mesh_update_mutex);
            if (pending_mesh_updates.contains(pos)) {
                continue;
            }
        }

        const Chunk& chunk = chunks.at(pos);
        if (chunk.object) {
            scene.remove_object_from_scene(chunk.object.get());
        }
        if (chunk.mesh) {
            renderer.destroy_mesh(*chunk.mesh);
        }
        chunks.erase(pos);
    }

    // Queue dirty CPU meshing and apply completed results on the render thread.
    std::vector<Mesh> old_meshes;

    {
        std::lock_guard lock(mesh_update_mutex);
        while (!completed_mesh_updates.empty()) {
            CompletedMeshUpdate completed = std::move(completed_mesh_updates.front());
            completed_mesh_updates.pop();
            pending_mesh_updates.erase(completed.pos);

            auto chunk_it = chunks.find(completed.pos);
            if (chunk_it == chunks.end()) {
                continue;
            }

            Chunk& chunk = chunk_it->second;
            if (chunk.mesh_revision != completed.revision || chunk.lod != completed.lod) {
                continue;
            }

            MeshData mesh_data = std::move(completed.mesh_data);
            bool empty_chunk = mesh_data.vertices.empty() || mesh_data.indices.empty();
            if (chunk.object) {
                scene.remove_object_from_scene(chunk.object.get());
            }
            if (chunk.mesh) {
                old_meshes.push_back(std::move(*chunk.mesh));
                chunk.mesh.reset();
            }

            if (empty_chunk) {
                if (chunk.object) {
                    chunk.object->mesh = nullptr;
                }
                chunk.dirty = false;
                continue;
            }

            chunk.mesh = std::make_unique<Mesh>(renderer.load_mesh(std::move(mesh_data)));
            if (!chunk.object) {
                chunk.object = std::make_unique<Object>(
                    chunk.mesh.get(),
                    atlas_material.get(),
                    glm::vec3(completed.pos.x * CHUNK_SIZE_X, completed.pos.y * CHUNK_SIZE_Y, completed.pos.z * CHUNK_SIZE_Z),
                    glm::vec3(0.0f, 0.0f, 0.0f)
                );
            } else {
                chunk.object->mesh = chunk.mesh.get();
            }
            scene.add_object_to_scene(chunk.object.get());
            chunk.dirty = false;
        }
    }

    for (auto& [pos, chunk] : chunks) {
        const ChunkLOD desired_lod = lod_for_chunk_distance(
            pos.x - camera_chunk_x,
            pos.y - camera_chunk_y,
            pos.z - camera_chunk_z
        );
        if (chunk.lod != desired_lod) {
            chunk.lod = desired_lod;
            chunk.dirty = true;
        }
        if (!chunk.dirty)
            continue;

        std::lock_guard lock(mesh_update_mutex);
        if (pending_mesh_updates.contains(pos)) {
            continue;
        }
        pending_mesh_updates.insert(pos);
        mesh_update_queue.push({pos, chunk.lod, chunk.mesh_revision, chunk.copy_blocks()});
        mesh_update_condition.notify_one();
    }

    // All new meshes have now been loaded.
    // Now the old Mesh objects can safely be destroyed.
    renderer.destroy_meshes(old_meshes);


    // Frustum culling
    const float half_fov = glm::radians(scene.fovy * 0.7); // dont use 0.5, since then it culls to early

    // Approximate the chunk with a bounding sphere.
    const float half_x = CHUNK_SIZE_X * 0.5f;
    const float half_y = CHUNK_SIZE_Y * 0.5f;
    const float half_z = CHUNK_SIZE_Z * 0.5f;
    const float chunk_radius = std::sqrt(half_x * half_x + half_y * half_y + half_z * half_z);

    for (const auto& [pos, chunk] : chunks) {
        if (!chunk.object) {
            continue;
        }

        glm::vec3 chunk_center((pos.x + 0.5f) * CHUNK_SIZE_X, (pos.y + 0.5f) * CHUNK_SIZE_Y, (pos.z + 0.5f) * CHUNK_SIZE_Z);

        glm::vec3 to_chunk = chunk_center - scene.cam_pos;
        float distance = glm::length(to_chunk);

        // Camera is inside/very close to the chunk.
        if (distance <= chunk_radius) {
            chunk.object->visible = true;
            continue;
        }

        glm::vec3 direction = to_chunk / distance;
        float angle = glm::dot(camera_forward, direction);

        // Expand the viewing cone by the angular radius of the chunk.
        float angular_radius = std::asin(std::min(1.0f, chunk_radius / distance));
        float min_angle = std::cos(half_fov + angular_radius);

        chunk.object->visible = angle >= min_angle;
    }

}

void World::setup() {
    
    std::cout << "Loading resources...\n";
    atlas_texture = std::make_unique<Texture>(
        renderer.load_texture("assets/blocks.ktx")
    );
    std::cout << "Loading shader...\n";
    shader = std::make_unique<Shader>(
        renderer.load_shader("assets/shader.slang")
    );

    std::cout << "Creating material...\n";

    atlas_material = std::make_unique<Material>(
        atlas_texture.get(),
        shader.get()
    );

    std::cout << "Configuring scene..\n";
    scene.cam_pos = glm::vec3(18.0f, 500.0f, 42.0f);
    scene.light_pos = glm::vec3(-80.0f, 140.0f, 40.0f);
    scene.clear_color = glm::vec4(0.10f, 0.20f, 0.32f, 1.0f);
    scene.far_plane = static_cast<float>((RENDER_DISTANCE + 2) * 2 * CHUNK_SIZE_X);

    update_chunks();
}

void World::update(float delta_time) {
    update_chunks();
    // Remove Elevation tiles that are too far away
    std::vector<TileCoordinate> tiles_to_remove;
    for (const auto& [coord, tile] : elevation_tiles) {
        int dx = coord.x - geo_to_tile(world_to_geo(static_cast<int>(scene.cam_pos.x), static_cast<int>(scene.cam_pos.z)), ELEVATION_ZOOM).x;
        int dz = coord.y - geo_to_tile(world_to_geo(static_cast<int>(scene.cam_pos.x), static_cast<int>(scene.cam_pos.z)), ELEVATION_ZOOM).y;
        if (std::abs(dx) > ELEVATION_TILE_CACHE_DISTANCE || std::abs(dz) > ELEVATION_TILE_CACHE_DISTANCE) {
            tiles_to_remove.push_back(coord);
        }
    }

    for (const TileCoordinate& coord : tiles_to_remove) {
        elevation_tiles.erase(coord);
    }

}

Scene& World::get_scene() {
    return scene;
}

BlockType World::get_block(int x, int y, int z) {
    const int chunk_x = static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_SIZE_X));
    const int chunk_y = static_cast<int>(std::floor(static_cast<float>(y) / CHUNK_SIZE_Y));
    const int chunk_z = static_cast<int>(std::floor(static_cast<float>(z) / CHUNK_SIZE_Z));

    const int block_x = x - chunk_x * CHUNK_SIZE_X;
    const int block_y = y - chunk_y * CHUNK_SIZE_Y;
    const int block_z = z - chunk_z * CHUNK_SIZE_Z;

    const ChunkPos pos {chunk_x, chunk_y, chunk_z};

    // Due to multithreading chunk may not exist yet, so we return air if it doesn't exist
    if (!chunks.contains(pos)) {
        return BlockType::Air;
    }
    Chunk& chunk = chunks.at(pos);
    return chunk.get_block(block_x, block_y, block_z);
}

void World::set_block(int x, int y, int z, BlockType block) {
    const int chunk_x = static_cast<int>(std::floor(
        static_cast<float>(x) / CHUNK_SIZE_X
    ));
    const int chunk_y = static_cast<int>(std::floor(
        static_cast<float>(y) / CHUNK_SIZE_Y
    ));
    const int chunk_z = static_cast<int>(std::floor(
        static_cast<float>(z) / CHUNK_SIZE_Z
    ));

    const int block_x = x - chunk_x * CHUNK_SIZE_X;
    const int block_y = y - chunk_y * CHUNK_SIZE_Y;
    const int block_z = z - chunk_z * CHUNK_SIZE_Z;

    const ChunkPos pos {chunk_x, chunk_y, chunk_z};

    Chunk& chunk = chunks.at(pos);
    chunk.set_block(block_x, block_y, block_z, block);

    auto mark_neighbor_dirty = [this](ChunkPos neighbor_pos) {
        if (auto neighbor = chunks.find(neighbor_pos); neighbor != chunks.end()) {
            neighbor->second.dirty = true;
        }
    };

    if (block_x == 0) {
        mark_neighbor_dirty({chunk_x - 1, chunk_y, chunk_z});
    }
    if (block_x == CHUNK_SIZE_X - 1) {
        mark_neighbor_dirty({chunk_x + 1, chunk_y, chunk_z});
    }
    if (block_y == 0) {
        mark_neighbor_dirty({chunk_x, chunk_y - 1, chunk_z});
    }
    if (block_y == CHUNK_SIZE_Y - 1) {
        mark_neighbor_dirty({chunk_x, chunk_y + 1, chunk_z});
    }
    if (block_z == 0) {
        mark_neighbor_dirty({chunk_x, chunk_y, chunk_z - 1});
    }
    if (block_z == CHUNK_SIZE_Z - 1) {
        mark_neighbor_dirty({chunk_x, chunk_y, chunk_z + 1});
    }
}