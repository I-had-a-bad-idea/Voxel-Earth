#include "World.h"

World::World() 
    : noise(1234, 0.01f)
{}

void World::setup(Renderer& renderer) {
    
    std::cout << "Loading resources...\n";
    atlas_texture = std::make_unique<Texture>(
        renderer.load_texture("assets/blocks.ktx")
    );
    std::cout << "Loading shader...\n";
    shader = std::make_unique<Shader>(
        renderer.load_shader("external/VGL/assets/shader.slang")
    );

    std::cout << "Creating material...\n";

    gravel_material = std::make_unique<Material>(
        atlas_texture.get(),
        shader.get()
    );


    chunks.resize(WORLD_SIZE_X);
    std::cout << "Creating chunks...\n";
    for (int chunk_x = 0; chunk_x < WORLD_SIZE_X; chunk_x++) {
        chunks[chunk_x].resize(WORLD_SIZE_Z);
        for (int chunk_z = 0; chunk_z < WORLD_SIZE_Z; chunk_z++) {
            std::cout << "Chunk " << chunk_x * WORLD_SIZE_Z + (chunk_z+1) << " of " << WORLD_SIZE_X * WORLD_SIZE_Z << std::endl;
            chunks[chunk_x][chunk_z] = Chunk(noise, chunk_x, chunk_z);
        }
    }


    std::cout << "Adding chunks to scene...\n";
    for (int chunk_x = 0; chunk_x < WORLD_SIZE_X; chunk_x++) {
        for (int chunk_z = 0; chunk_z < WORLD_SIZE_Z; chunk_z++) {
            std::cout << "Generating mesh data...\n";
            MeshData mesh_data = chunks[chunk_x][chunk_z].generate_mesh_data();
            std::cout << "Creating mesh...\n";
            chunks[chunk_x][chunk_z].mesh = std::make_unique<Mesh>(renderer.load_mesh(mesh_data));
            std::cout << "Creating object...\n";
            chunks[chunk_x][chunk_z].object = std::make_unique<Object>(
                Object(chunks[chunk_x][chunk_z].mesh.get(), gravel_material.get(),
                glm::vec3(chunk_x * CHUNK_SIZE_X, 0.0f, chunk_z * CHUNK_SIZE_Z), glm::vec3(0.0f, 0.0f, 0.0f)) 
            );
            std::cout << "Adding to scene...\n";
            scene.add_object_to_scene(chunks[chunk_x][chunk_z].object.get());
        }
    }

    std::cout << "Configuring scene..\n";
    scene.cam_pos = glm::vec3(0.0f, -50.0f, 5.0f);
    scene.far_plane = 100.0f;
}

void World::update(float delta_time) {
}

Scene& World::get_scene() {
    return scene;
}