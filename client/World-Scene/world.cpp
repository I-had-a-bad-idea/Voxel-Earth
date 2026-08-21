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

    for (float x = 0; x < 100; x+=2) {
        for (float z = 0; z < 100; z+=2) {
            float height = noise.at(x, z);
            // Create a cube object at the given position
            std::unique_ptr<Object> cube = std::make_unique<Object>(
                cube_mesh.get(),
                gravel_material.get(),
                glm::vec3(x, height, z),
                glm::vec3(0.0f, 0.0f, 0.0f)
            );
            // Add to the cubes vector
            cubes.push_back(std::move(cube));
        }
    }


    std::cout << "Adding object(s) to scene...\n";
    for (auto& cube : cubes) {
        scene.add_object_to_scene(cube.get());
    }

    std::cout << "Configuring scene..\n";
    scene.cam_pos = glm::vec3(0.0f, 0.0f, 5.0f);
    scene.far_plane = 100.0f;
}

void World::update(float delta_time) {
}

Scene& World::get_scene() {
    return scene;
}