#ifndef WORLD_H
#define WORLD_H

#include <VGL/object.h>
#include <VGL/renderer.h>

class World {
    Scene scene;
    
    public:
        void setup(Renderer& renderer);
        const Scene& get_scene();
};


#endif