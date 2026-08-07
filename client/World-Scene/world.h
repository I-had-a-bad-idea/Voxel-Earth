#ifndef WORLD_H
#define WORLD_H

#include <external/VulkanGraphicsLib/include/object.h>
#include <external/VulkanGraphicsLib/include/renderer.h>

class World {
    Scene scene;
    
    public:
        void setup(Renderer& renderer);
        const Scene& get_scene();
};


#endif