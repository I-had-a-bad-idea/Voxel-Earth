#include "World.h"

World::World() 
    : noise(1234, 0.01f, 4, 2.0f, 0.5f)
{}

void World::update_chunks(Renderer& renderer) {
    int camera_chunk_x = static_cast<int>(std::floor(scene.cam_pos.x / CHUNK_SIZE_X));
    int camera_chunk_z = static_cast<int>(std::floor(scene.cam_pos.z / CHUNK_SIZE_Z));

    for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; dx++) {
        for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; dz++) {
            int chunk_x = camera_chunk_x + dx;
            int chunk_z = camera_chunk_z + dz;

            ChunkPos pos {chunk_x, chunk_z};

            if (chunks.contains(pos)) {
                continue; // Already generated
            }

            std::cout
                << "Generating chunk "
                << chunk_x << ", " << chunk_z << "\n";

            auto [it, inserted]  = chunks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(pos),
                std::forward_as_tuple(noise, chunk_x, chunk_z)
            );

            Chunk& chunk = it->second;

            MeshData mesh_data = chunk.generate_mesh_data();
            chunk.mesh = std::make_unique<Mesh>(renderer.load_mesh(mesh_data));

            chunk.object = std::make_unique<Object>(chunk.mesh.get(), atlas_material.get(),
                glm::vec3(chunk_x * CHUNK_SIZE_X, 0.0f, chunk_z * CHUNK_SIZE_Z), glm::vec3(0.0f, 0.0f, 0.0f));
            
            scene.add_object_to_scene(chunk.object.get());
        }
    }


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
        std::cout << "Removing chunk " << pos.x << ", " << pos.z << "\n";
        const Chunk& chunk = chunks.at(pos);
        scene.remove_object_from_scene(chunk.object.get());
        renderer.destroy_mesh(*chunk.mesh);
        chunks.erase(pos);
    }
}

void World::setup(Renderer& renderer) {
    
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
    scene.cam_pos = glm::vec3(0.0f, -50.0f, 5.0f);
    scene.far_plane = 1000.0f;

    update_chunks(renderer);
}

void World::update(Renderer& renderer, float delta_time) {
    update_chunks(renderer);
}

Scene& World::get_scene() {
    return scene;
}