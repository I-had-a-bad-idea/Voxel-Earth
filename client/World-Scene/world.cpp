#include "World.h"


void World::setup(Renderer& renderer) {
    // Load meshes, textures and shaders
    std::cout << "Loading resources...\n";
    Mesh cube_mesh = renderer.load_mesh("external/VulkanGraphicsLib/assets/monkey.obj"); // Currently only .obj is supported
    Texture gravel_texture = renderer.load_texture("external/VulkanGraphicsLib/assets/Textures/Gravel.ktx"); // Currently only .ktx (as it is a format the GPU likes)
    std::cout << "Loading shader...\n";
    Shader shader = renderer.load_shader("external/VulkanGraphicsLib/assets/shader.slang"); // The slang compiler is included in the library and shaders will be compiled when loaded
    std::cout << "Creating material...\n";
    // Create a material for gravel
    Material gravel_material(&gravel_texture, &shader); // Create a material from a texture and a shader

    // Create object(s) (mesh, material, position, rotation)
    Object cube(&cube_mesh, &gravel_material, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f)); 

    // add objects to scene
    std::cout << "Adding object(s) to scene...\n";
    scene.add_object_to_scene(&cube);
}

const Scene& World::get_scene() {
    return scene;
}