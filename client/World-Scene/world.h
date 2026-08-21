#ifndef WORLD_H
#define WORLD_H

#include <VGL/object.h>
#include <VGL/renderer.h>

#include "Math/noise.h"

class World {
    Scene scene;

    std::unique_ptr<Mesh> cube_mesh;
    std::unique_ptr<Texture> gravel_texture;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Material> gravel_material;
    std::unique_ptr<Object> cube;
    
    Noise noise;

    public:
        void setup(Renderer& renderer);
        void update(float delta_time);
        Scene& get_scene();
};


#endif