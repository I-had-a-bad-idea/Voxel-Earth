#pragma once

#include <VGL/object.h>

struct RemotePlayer {
    uint32_t id;
    glm::vec3 position;
};

struct PlayerObject {
    uint32_t player_id;
    std::unique_ptr<Object> object;
};