#include "World.h"

namespace {
ChunkLOD lod_for_chunk_distance(int dx, int dy, int dz) {
    return ChunkLOD::LOD0;
    // const int distance = std::max(std::abs(dx), std::max(std::abs(dy), std::abs(dz)));
    // if (distance <= 2) {
    //     return ChunkLOD::LOD0;
    // }
    // if (distance <= 3) {
    //     return ChunkLOD::LOD1;
    // }
    // if (distance <= 5) {
    //     return ChunkLOD::LOD2;
    // }
    // if (distance <= 7) {
    //     return ChunkLOD::LOD3;
    // }
    // if (distance <= 9) {
    //     return ChunkLOD::LOD4;
    // }
    // if (distance <= 11) {
    //     return ChunkLOD::LOD5;
    // }
    // return ChunkLOD::LOD6;
}
}

World::World(Renderer& renderer_)
    : renderer(renderer_)
{
    world_data_thread = std::thread(&World::fetch_elevation_tiles, this);
    world_cover_data_thread = std::thread(&World::fetch_world_cover_tiles, this);
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
        std::lock_guard lock(terrain_cache_mutex);
        stop_world_data_thread = true;
    }
    elevation_tile_condition.notify_one();
    world_data_thread.join();
    world_cover_tile_condition.notify_one();
    world_cover_data_thread.join();

    {
        std::lock_guard lock(mesh_update_mutex);
        stop_mesh_updates = true;
    }
    mesh_update_condition.notify_one();
    mesh_update_thread.join();
}

void World::fetch_elevation_tiles() {
    while (true) {
        ElevationTileCoordinate coord;
        {
            std::unique_lock lock(terrain_cache_mutex);
            elevation_tile_condition.wait(lock, [this] {
                return stop_world_data_thread || !elevation_tile_queue.empty();
            });

            if (stop_world_data_thread && elevation_tile_queue.empty()) {
                return;
            }

            coord = elevation_tile_queue.front();
            elevation_tile_queue.pop();
        }

        ElevationTile tile_data;
        try {
            tile_data = elevation_tile_fetcher.elevation_tile_fetch(ELEVATION_ZOOM, coord.x, coord.y);
        } catch (const std::exception& error) {
            std::cerr << "Failed to load elevation tile " << coord.x << ", " << coord.y
                      << ": " << error.what() << ". Using sea level.\n";
        }

        {
            std::lock_guard lock(terrain_cache_mutex);
            elevation_tiles.emplace(coord, std::move(tile_data));
            requested_elevation_tiles.erase(coord);
        }
        elevation_tile_condition.notify_all();
    }
}

void World::fetch_world_cover_tiles() {
    while (true) {
        WorldCoverTileCoordinate coord;
        {
            std::unique_lock lock(terrain_cache_mutex);
            world_cover_tile_condition.wait(lock, [this] {
                return stop_world_data_thread || !world_cover_tile_queue.empty();
            });

            if (stop_world_data_thread && world_cover_tile_queue.empty()) {
                return;
            }

            coord = world_cover_tile_queue.front();
            world_cover_tile_queue.pop();
        }

        WorldCoverTile tile_data;
        try {
            const int tile_lat = world_cover_tile_lat(coord.x);
            const int tile_lon = world_cover_tile_lon(coord.y);
            tile_data = world_cover_fetcher.world_cover_tile_fetch(tile_lat, tile_lon);
        } catch (const std::exception& error) {
            std::cerr << "Failed to load world cover tile " << coord.x << ", " << coord.y
                      << ": " << error.what() << ". Using elevation-based surface.\n";
        }

        {
            std::lock_guard lock(terrain_cache_mutex);
            world_cover_tiles.emplace(coord, std::move(tile_data));
            requested_world_cover_tiles.erase(coord);
        }
        world_cover_tile_condition.notify_all();
    }
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

float World::get_elevation_height(ElevationTileCoordinate coord, int pixel_x, int pixel_y) {
    std::unique_lock lock(terrain_cache_mutex);
    auto it = elevation_tiles.find(coord);
    if (it == elevation_tiles.end()) {
        if (requested_elevation_tiles.insert(coord).second) {
            elevation_tile_queue.push(coord);
            elevation_tile_condition.notify_one();
        }
        elevation_tile_condition.wait(lock, [this, coord] {
            return elevation_tiles.contains(coord);
        });
        it = elevation_tiles.find(coord);
    }
    return it->second.get(pixel_x, pixel_y);
}

LandCover World::get_world_cover(GeoCoordinate geo) {
    const WorldCoverTileCoordinate coord{
        world_cover_tile_lat(geo.latitude),
        world_cover_tile_lon(geo.longitude)
    };

    std::unique_lock lock(terrain_cache_mutex);
    auto it = world_cover_tiles.find(coord);
    if (it == world_cover_tiles.end()) {
        if (requested_world_cover_tiles.insert(coord).second) {
            world_cover_tile_queue.push(coord);
            world_cover_tile_condition.notify_one();
        }
        world_cover_tile_condition.wait(lock, [this, coord] {
            return world_cover_tiles.contains(coord);
        });
        it = world_cover_tiles.find(coord);
    }

    const WorldCoverTile& tile = it->second;
    if (tile.width <= 0 || tile.height <= 0 || tile.land_cover.empty()) {
        return LandCover::NoData;
    }

    const double x_fraction = (geo.longitude - coord.y) / 3.0;
    const double y_fraction = (coord.x + 3.0 - geo.latitude) / 3.0;
    const int pixel_x = std::clamp(static_cast<int>(x_fraction * tile.width), 0, tile.width - 1);
    const int pixel_y = std::clamp(static_cast<int>(y_fraction * tile.height), 0, tile.height - 1);
    return tile.get_land_cover(pixel_x, pixel_y);
}


TerrainColumn World::generate_terrain_column(int world_x, int world_z) {
    const GeoCoordinate geo = world_to_geo(world_x, world_z);
    
    // Elevation
    const ElevationTileCoordinate tile = geo_to_elevation_tile(geo, ELEVATION_ZOOM);
    const ElevationTileCoordinate pixel = geo_to_elevation_tile_pixel(geo, ELEVATION_ZOOM);
    const float height_f = get_elevation_height(tile, pixel.x, pixel.y);


    TerrainColumn column;
    column.height = static_cast<int>(std::round(height_f / METERS_PER_WORLD_BLOCK));

    // World cover

    const LandCover land_cover = get_world_cover(geo);

    column.biome = Biome::Plains;
    column.land_cover = land_cover;
    column.surface = BlockType::Grass;
    if (land_cover == LandCover::SnowIce) {
        column.biome = Biome::Tundra;
        column.surface = BlockType::Snow;
    } else if (land_cover == LandCover::BareSparseVegetation) {
        column.biome = Biome::Desert;
        column.surface = BlockType::Sand;
    } else if (land_cover == LandCover::BuiltUp) {
        column.surface = BlockType::Stone;
    } else if (land_cover == LandCover::PermanentWater || column.height <= SEA_LEVEL + 2) {
        column.surface = BlockType::Sand;
    } else if (land_cover == LandCover::TreeCover || land_cover == LandCover::Shrubland) {
        column.biome = Biome::Forest;
    } else if (land_cover == LandCover::NoData && column.height > SEA_LEVEL + 2) {
        column.surface = column.height > CHUNK_SIZE_Y * 0.72f ? BlockType::Snow : BlockType::Grass;
        if (column.surface == BlockType::Snow) {
            column.biome = Biome::Tundra;
        }
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
    while (uploaded_chunks < 16) {
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

        // Handle pending block edits for that chunk
        if (auto pending = pending_block_edits.find(generated.pos); pending != pending_block_edits.end()) {
            for (const PendingBlockEdit& edit : pending->second) {
                it->second.set_block(edit.x, edit.y, edit.z, edit.block);
            }
            // Regenerate the mesh
            generated.mesh_data = it->second.generate_mesh_data(generated.lod);
            pending_block_edits.erase(pending); // And remove the pending block edits
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
            chunk.in_scene = true;
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

    const ChunkPos camera_chunk {camera_chunk_x, camera_chunk_y, camera_chunk_z};
    const bool camera_chunk_changed = !has_stream_camera_chunk ||
        camera_chunk != last_stream_camera_chunk;

    if (camera_chunk_changed) {
        last_stream_camera_chunk = camera_chunk;
        has_stream_camera_chunk = true;

        generation_camera_chunk_x.store(camera_chunk_x, std::memory_order_relaxed);
        generation_camera_chunk_y.store(camera_chunk_y, std::memory_order_relaxed);
        generation_camera_chunk_z.store(camera_chunk_z, std::memory_order_relaxed);

        new_requests.push_back({camera_chunk_x, 0, camera_chunk_z});
        for (int y = camera_chunk_y - VERTICAL_RENDER_DISTANCE; y <= camera_chunk_y + VERTICAL_RENDER_DISTANCE; ++y) {
            new_requests.push_back({camera_chunk_x, y, camera_chunk_z});
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

                new_requests.push_back({x, 0, z});
                int start_y = camera_chunk_y - VERTICAL_RENDER_DISTANCE;
                if (step > UNDERGROUND_STREAM_DISTANCE) {
                    start_y = std::max(0, start_y); // dont generate chunks underground
                }

                for (int y = start_y; y <= camera_chunk_y + VERTICAL_RENDER_DISTANCE; ++y) {
                    new_requests.push_back({x, y, z});
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
    }

    // Queue some new chunk requests
    int requests_this_frame = 0;
    while (next_new_request < new_requests.size() && requests_this_frame < MAX_NEW_REQUESTS_PER_FRAME) {
        queue_chunk_generation(new_requests[next_new_request]);
        next_new_request++;
        requests_this_frame++;
        
    }

    if (next_new_request == new_requests.size()) {
        new_requests.clear();
        next_new_request = 0;
    }

    process_completed_chunks();


    std::vector<Mesh> old_meshes;

    if (camera_chunk_changed) {
        // Remove chunks that are too far away.
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

            Chunk& chunk = chunks.at(pos);
            if (chunk.object) {
                if (chunk.in_scene) {
                    scene.remove_object_from_scene(chunk.object.get());
                }
            }
            if (chunk.mesh) {
                old_meshes.push_back(std::move(*chunk.mesh));
                chunk.mesh.reset();
            }
            chunks.erase(pos);
        }
    }

    // Queue dirty CPU meshing and apply completed results on the render thread.
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
                if (chunk.in_scene) {
                    scene.remove_object_from_scene(chunk.object.get());
                    chunk.in_scene = false;
                }
            }
            if (chunk.mesh) {
                old_meshes.push_back(std::move(*chunk.mesh));
                chunk.mesh.reset();
            }

            if (empty_chunk) {
                if (chunk.object) {
                    chunk.object->mesh = nullptr;
                }
                chunk.in_scene = false;
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
            chunk.in_scene = true;
            chunk.dirty = false;
        }
    }

    // Update dirty chunks and LOD
    if (camera_chunk_changed || mesh_updates_needed) {
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
        mesh_updates_needed = false;
    }

    // All new meshes have now been loaded.
    // Now the old Mesh objects can safely be destroyed.
    renderer.destroy_meshes(old_meshes);


    // Frustum culling

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

        chunk.object->visible = sphere_in_frustum(scene.frustum, chunk_center, chunk_radius);
    }
    prefetch_elevation_tiles(camera_chunk_x, camera_chunk_z);
}

void World::prefetch_elevation_tiles(int camera_chunk_x, int camera_chunk_z) {
    bool added_tiles = false;
    // Fetch all elevation tiles that are within the render distance of the camera.
    for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; ++dx) {
        for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; ++dz) {
            int chunk_x = camera_chunk_x + dx;
            int chunk_z = camera_chunk_z + dz;
            const GeoCoordinate geo = world_to_geo(chunk_x * CHUNK_SIZE_X, chunk_z * CHUNK_SIZE_Z);
            const ElevationTileCoordinate tile = geo_to_elevation_tile(geo, ELEVATION_ZOOM);
            std::lock_guard lock(terrain_cache_mutex);
            // Already downloaded
            if (elevation_tiles.contains(tile)) {
                continue;
            }

            // Already queued
            if (!requested_elevation_tiles.insert(tile).second) {
                continue;
            }
            // Add it to the queue
            elevation_tile_queue.push(tile);
            added_tiles = true;
        }
    }
    if (added_tiles) {
        elevation_tile_condition.notify_one();
    }
}

void World::setup() {
    
    std::cout << "Loading resources...\n";
    player_mesh = std::make_unique<Mesh>(
        renderer.load_mesh(generate_player_mesh())
    );

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
    scene.cam_pos = glm::vec3(18.0f, 1250.0f, 42.0f);
    scene.light_pos = glm::vec3(-80.0f, 140.0f, 40.0f);
    scene.clear_color = glm::vec4(0.10f, 0.20f, 0.32f, 1.0f);
    scene.far_plane = static_cast<float>((RENDER_DISTANCE + 2) * 2 * CHUNK_SIZE_X);

    update_chunks();
}

void World::update(float delta_time) {
    update_chunks();
    // Remove Elevation tiles that are too far away
    std::vector<ElevationTileCoordinate> tiles_to_remove;
    std::lock_guard lock(terrain_cache_mutex);
    for (const auto& [coord, tile] : elevation_tiles) {
        int dx = coord.x - geo_to_elevation_tile(world_to_geo(static_cast<int>(scene.cam_pos.x), static_cast<int>(scene.cam_pos.z)), ELEVATION_ZOOM).x;
        int dz = coord.y - geo_to_elevation_tile(world_to_geo(static_cast<int>(scene.cam_pos.x), static_cast<int>(scene.cam_pos.z)), ELEVATION_ZOOM).y;
        if (std::abs(dx) > ELEVATION_TILE_CACHE_DISTANCE || std::abs(dz) > ELEVATION_TILE_CACHE_DISTANCE) {
            tiles_to_remove.push_back(coord);
        }
    }

    for (const ElevationTileCoordinate& coord : tiles_to_remove) {
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

    if (!chunks.contains(pos)) {
        auto& pending_edits = pending_block_edits[pos]; // get existing PendingBlockEdits for that chunk
        // Check if tthere already is an existing PendingBlockEdit for that position
        auto existing = std::find_if(pending_edits.begin(), pending_edits.end(),
            [block_x, block_y, block_z](const PendingBlockEdit& edit) {
                return edit.x == block_x && edit.y == block_y && edit.z == block_z;
            });
        if (existing != pending_edits.end()) {
            existing->block = block; // if yes replace it
        } else {
            pending_edits.push_back({block_x, block_y, block_z, block}); // if no add a new one
        }
        return;
    }

    Chunk& chunk = chunks.at(pos);
    chunk.set_block(block_x, block_y, block_z, block);
    mesh_updates_needed = true;

    auto mark_neighbor_dirty = [this](ChunkPos neighbor_pos) {
        if (auto neighbor = chunks.find(neighbor_pos); neighbor != chunks.end()) {
            neighbor->second.dirty = true;
            mesh_updates_needed = true;
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

glm::vec3 correct_player_object_position(glm::vec3 position) {
    position.y -= 2 * player_height;
    position.x -= 2 * player_half_width;
    
    return position;
}

void World::add_player_object(uint32_t player_id, glm::vec3 position) {
    position = correct_player_object_position(position);
    // Create a new player object and add it to the scene
    auto player_object = std::make_unique<Object>(
        player_mesh.get(),
        atlas_material.get(),
        position,
        glm::vec3(0.0f, 0.0f, 0.0f)
    );
    scene.add_object_to_scene(player_object.get());
    player_objects.push_back({player_id, std::move(player_object)});
}

void World::update_player_object(uint32_t player_id, glm::vec3 position) {
    position = correct_player_object_position(position);
    // Find the player object with the given ID and update its position
    for (auto& player : player_objects) {
        if (player.player_id == player_id) {
            player.object->position = position;
            return;
        }
    }
}
void World::remove_player_object(uint32_t player_id) {
    // Find the player object with the given ID and remove it from the scene
    for (auto it = player_objects.begin(); it != player_objects.end(); ++it) {
        if (it->player_id == player_id) {
            scene.remove_object_from_scene(it->object.get());
            player_objects.erase(it);
            return;
        }
    }
}


MeshData generate_player_mesh() {
    MeshData mesh_data;

    BlockTexture top_block = get_block_texture(BlockType::Grass);
    BlockTexture bottom_block = get_block_texture(BlockType::Dirt);

    const AtlasTile bottom_side = bottom_block.side;
    const AtlasTile bottom_top  = bottom_block.top;
    const AtlasTile bottom_bottom = bottom_block.bottom;

    const AtlasTile top_side = top_block.side;
    const AtlasTile top_top  = top_block.top;
    const AtlasTile top_bottom = top_block.bottom;

    auto add_face_quad = [&](const glm::uvec3& v0, const glm::uvec3& v1, const glm::uvec3& v2, const glm::uvec3& v3,
                             PackedNormal normal, AtlasTile tile) {
        
        const uint32_t start = static_cast<uint32_t>(mesh_data.vertices.size());
        const uint32_t packed_normal = static_cast<uint32_t>(normal);
        const uint32_t packed_tile = pack_atlas_tile(tile.x, tile.y);

        mesh_data.vertices.push_back({pack_pos(v0.x, v0.y, v0.z), packed_normal, pack_uv(0, 0), packed_tile});
        mesh_data.vertices.push_back({pack_pos(v1.x, v1.y, v1.z), packed_normal, pack_uv(1, 0), packed_tile});
        mesh_data.vertices.push_back({pack_pos(v2.x, v2.y, v2.z), packed_normal, pack_uv(1, 1), packed_tile});
        mesh_data.vertices.push_back({pack_pos(v3.x, v3.y, v3.z), packed_normal, pack_uv(0, 1), packed_tile});

        mesh_data.indices.push_back(start + 0);
        mesh_data.indices.push_back(start + 1);
        mesh_data.indices.push_back(start + 2);

        mesh_data.indices.push_back(start + 2);
        mesh_data.indices.push_back(start + 3);
        mesh_data.indices.push_back(start + 0);
    };

    auto add_cube = [&](uint32_t y0, AtlasTile side, AtlasTile top, AtlasTile bottom, bool include_bottom) {
        const uint32_t x0 = 0;
        const uint32_t x1 = 1;
        const uint32_t z0 = 0;
        const uint32_t z1 = 1;
        const uint32_t y1 = y0 + 1;

        // -X
        add_face_quad({x0, y0, z1}, {x0, y0, z0},
            {x0, y1, z0}, {x0, y1, z1},
            PackedNormal::NegX, side);

        // +X
        add_face_quad({x1, y0, z0}, {x1, y0, z1},
            {x1, y1, z1}, {x1, y1, z0},
            PackedNormal::PosX, side);

        // -Z
        add_face_quad({x1, y0, z0}, {x0, y0, z0},
            {x0, y1, z0}, {x1, y1, z0},
            PackedNormal::NegZ, side);

        // +Z
        add_face_quad({x0, y0, z1}, {x1, y0, z1},
            {x1, y1, z1}, {x0, y1, z1},
            PackedNormal::PosZ, side);

        // Bottom
        // Don't generate this for the top cube because it touches the top face of the bottom cube.
        if (include_bottom) {
            add_face_quad({x0, y0, z0}, {x1, y0, z0},
                {x1, y0, z1}, {x0, y0, z1},
                PackedNormal::NegY, bottom);
        }

        // Top
        add_face_quad({x0, y1, z1}, {x1, y1, z1},
            {x1, y1, z0}, {x0, y1, z0},
            PackedNormal::PosY, top);
    };

    add_cube(0, bottom_side, bottom_top, bottom_bottom, true);
    add_cube(1, top_side, top_top, top_bottom, false); // no internal bottom face

    return mesh_data;
}