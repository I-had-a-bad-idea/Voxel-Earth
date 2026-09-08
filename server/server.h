#include <enet/enet.h>
#include <stdio.h>
#include <cstdint>

#include "network/network.h"

struct Player {
    ENetPeer* peer;
    uint32_t id;
    glm::vec3 position;
};

std::vector<Player> players;
uint32_t next_player_id = 1;

int main(void);