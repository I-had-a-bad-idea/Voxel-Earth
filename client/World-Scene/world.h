#ifndef WORLD_H
#define WORLD_H

#include <external/Rasterization-Renderer/Scenes/Scene.h>

class World : public Scene{
    public:
        void Update(RenderTarget& target, float delta_time);
        void Setup();
};


#endif