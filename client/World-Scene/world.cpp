#include "World.h"

World::World() 
    : noise(1234, 0.05f)
{}

void World::setup(Renderer& renderer) {
    
    std::cout << "Loading resources...\n";
    cube_mesh = std::make_unique<Mesh>(
        renderer.load_mesh("assets/cube.obj")
    );
    gravel_texture = std::make_unique<Texture>(
        renderer.load_texture("external/VGL/assets/Textures/Gravel.ktx")
    );
    std::cout << "Loading shader...\n";
    shader = std::make_unique<Shader>(
        renderer.load_shader("external/VGL/assets/shader.slang")
    );

    std::cout << "Creating material...\n";

    gravel_material = std::make_unique<Material>(
        gravel_texture.get(),
        shader.get()
    );


    std::cout << "Creating chunks...\n";
    for (int chunk_x = 0; chunk_x < 5; chunk_x++) {
        for (int chunk_z = 0; chunk_z < 5; chunk_z++) {
            chunks[chunk_x][chunk_z] = Chunk(noise, chunk_x, chunk_z);
        }
    }


    std::cout << "Adding object(s) to scene...\n";
    // for (auto& cube : cubes) {
    //     scene.add_object_to_scene(cube.get());
    // }

    std::cout << "Configuring scene..\n";
    scene.cam_pos = glm::vec3(0.0f, 0.0f, 5.0f);
    scene.far_plane = 100.0f;
}

void World::update(float delta_time) {
}

Scene& World::get_scene() {
    return scene;
}