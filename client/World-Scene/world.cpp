#include "World.h"

World::World(Renderer& renderer_)
    : renderer(renderer_),
      continental(1234, 0.0008f, 4, 2.0f, 0.5f, FastNoiseLite::FractalType_FBm),
      hills(5678, 0.006f, 4, 2.0f, 0.5f, FastNoiseLite::FractalType_FBm),
      mountains(9012, 0.0025f, 5, 2.1f, 0.55f, FastNoiseLite::FractalType_Ridged),
      temperature(3456, 0.0015f, 3, 2.0f, 0.5f, FastNoiseLite::FractalType_FBm),
      moisture(7890, 0.0015f, 3, 2.0f, 0.5f, FastNoiseLite::FractalType_FBm)
{
    generation_thread = std::thread(&World::generate_chunks, this);
}

World::~World() {
    {
        std::lock_guard lock(generation_mutex);
        stop_generation = true;
    }
    generation_condition.notify_one();
    generation_thread.join();
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

        // create chunk
        auto chunk = std::make_unique<Chunk>(
            continental,
            hills,
            mountains,
            temperature,
            moisture,
            pos.x,
            pos.z
        );
        MeshData mesh_data = chunk->generate_mesh_data(); // create mesh data

        {
            std::lock_guard lock(generation_mutex);
            completed_chunks.push({pos, std::move(chunk), std::move(mesh_data)}); // submit as completed
        }
    }
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
        chunk.mesh = std::make_unique<Mesh>(renderer.load_mesh(std::move(generated.mesh_data)));
        chunk.dirty = false;
        chunk.object = std::make_unique<Object>(
            chunk.mesh.get(),
            atlas_material.get(),
            glm::vec3(generated.pos.x * CHUNK_SIZE_X, 0.0f, generated.pos.z * CHUNK_SIZE_Z),
            glm::vec3(0.0f, 0.0f, 0.0f)
        );
        scene.add_object_to_scene(chunk.object.get()); // add to world
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
    int camera_chunk_z = static_cast<int>(std::floor(scene.cam_pos.z / CHUNK_SIZE_Z));


    ChunkPos current_chunk_pos{camera_chunk_x, camera_chunk_z};

    // Queue the current chunk first
    queue_chunk_generation(current_chunk_pos);

    // Generate outward in expanding rings
    for (int step = 1; step <= RENDER_DISTANCE; ++step) {
        int min_x = camera_chunk_x - step;
        int max_x = camera_chunk_x + step;
        int min_z = camera_chunk_z - step;
        int max_z = camera_chunk_z + step;

        // Bottom row
        for (int x = min_x; x <= max_x; ++x) {
            queue_chunk_generation(ChunkPos{x, min_z});
        }

        // Top row
        for (int x = min_x; x <= max_x; ++x) {
            queue_chunk_generation(ChunkPos{x, max_z});
        }

        // Left and right columns, excluding corners
        for (int z = min_z + 1; z < max_z; ++z) {
            queue_chunk_generation(ChunkPos{min_x, z});
            queue_chunk_generation(ChunkPos{max_x, z});
        }
    }

    process_completed_chunks();


    // Remove chunks that are too far away
    std::vector<ChunkPos> chunks_to_remove;
    for (const auto& [pos, chunk] : chunks) {
        int dx = pos.x - camera_chunk_x;
        int dz = pos.z - camera_chunk_z;
        if (std::abs(dx) > RENDER_DISTANCE || std::abs(dz) > RENDER_DISTANCE) {
            chunks_to_remove.push_back(pos);
        }
    }
    for (const ChunkPos& pos : chunks_to_remove) {
        const Chunk& chunk = chunks.at(pos);
        scene.remove_object_from_scene(chunk.object.get());
        renderer.destroy_mesh(*chunk.mesh);
        chunks.erase(pos);
    }

    // Process dirty chunks
    for (auto& [pos, chunk] : chunks) {
        if (chunk.dirty) {
            MeshData mesh_data = chunk.generate_mesh_data(); // create mesh data
            scene.remove_object_from_scene(chunk.object.get());
            // destroy old mesh
            renderer.destroy_mesh(*chunk.mesh);
            // load new mesh
            chunk.mesh = std::make_unique<Mesh>(renderer.load_mesh(std::move(mesh_data)));
            chunk.object.get()->mesh = chunk.mesh.get();
            scene.add_object_to_scene(chunk.object.get());
            chunk.dirty = false;
        }
    }


    // Frustum culling
    const float half_fov = glm::radians(scene.fovy * 0.7); // dont use 0.5, since then it culls to early

    // Approximate the chunk with a bounding sphere.
    const float half_x = CHUNK_SIZE_X * 0.5f;
    const float half_y = CHUNK_SIZE_Y * 0.5f;
    const float half_z = CHUNK_SIZE_Z * 0.5f;
    const float chunk_radius = std::sqrt(half_x * half_x + half_y * half_y + half_z * half_z);

    for (const auto& [pos, chunk] : chunks) {
        glm::vec3 chunk_center((pos.x + 0.5f) * CHUNK_SIZE_X, CHUNK_SIZE_Y * 0.5f, (pos.z + 0.5f) * CHUNK_SIZE_Z);

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
    scene.cam_pos = glm::vec3(18.0f, 50.0f, 42.0f);
    scene.light_pos = glm::vec3(-80.0f, 140.0f, 40.0f);
    scene.clear_color = glm::vec4(0.10f, 0.20f, 0.32f, 1.0f);
    scene.far_plane = static_cast<float>((RENDER_DISTANCE + 2) * CHUNK_SIZE_X) * 1.5f;

    update_chunks();
}

void World::update(float delta_time) {
    update_chunks();
}

Scene& World::get_scene() {
    return scene;
}

BlockType World::get_block(int x, int y, int z) {
    if (y >= CHUNK_SIZE_Y || y < 0) {
        return BlockType::Air; // everything above/below chunk is air
    }

    const int chunk_x = static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_SIZE_X));
    const int chunk_z = static_cast<int>(std::floor(static_cast<float>(z) / CHUNK_SIZE_Z));

    const int block_x = x - chunk_x * CHUNK_SIZE_X;
    const int block_z = z - chunk_z * CHUNK_SIZE_Z;

    const ChunkPos pos {chunk_x, chunk_z};

    // Due to multithreading chunk may not exist yet, so we return air if it doesn't exist
    if (!chunks.contains(pos)) {
        return BlockType::Air;
    }
    Chunk& chunk = chunks.at(pos);
    return chunk.get_block(block_x, y, block_z);
}

void World::set_block(int x, int y, int z, BlockType block) {
    const int chunk_x = static_cast<int>(std::floor(
        static_cast<float>(x) / CHUNK_SIZE_X
    ));
    const int chunk_z = static_cast<int>(std::floor(
        static_cast<float>(z) / CHUNK_SIZE_Z
    ));

    const int block_x = x - chunk_x * CHUNK_SIZE_X;
    const int block_z = z - chunk_z * CHUNK_SIZE_Z;

    const ChunkPos pos {chunk_x, chunk_z};

    Chunk& chunk = chunks.at(pos);
    chunk.set_block(block_x, y, block_z, block);

    auto mark_neighbor_dirty = [this](ChunkPos neighbor_pos) {
        if (auto neighbor = chunks.find(neighbor_pos); neighbor != chunks.end()) {
            neighbor->second.dirty = true;
        }
    };

    if (block_x == 0) {
        mark_neighbor_dirty({chunk_x - 1, chunk_z});
    }
    if (block_x == CHUNK_SIZE_X - 1) {
        mark_neighbor_dirty({chunk_x + 1, chunk_z});
    }
    if (block_z == 0) {
        mark_neighbor_dirty({chunk_x, chunk_z - 1});
    }
    if (block_z == CHUNK_SIZE_Z - 1) {
        mark_neighbor_dirty({chunk_x, chunk_z + 1});
    }
}