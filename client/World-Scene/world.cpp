#include "World.h"


void World::setup(Renderer& renderer) {
    std::cout << "Loading resources...\n";

    cube_mesh = std::make_unique<Mesh>(
        renderer.load_mesh("external/VGL/assets/monkey.obj")
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

    cube = std::make_unique<Object>(
        cube_mesh.get(),
        gravel_material.get(),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f)
    );

    std::cout << "Adding object(s) to scene...\n";

    scene.add_object_to_scene(cube.get());
}

const Scene& World::get_scene() {
    return scene;
}