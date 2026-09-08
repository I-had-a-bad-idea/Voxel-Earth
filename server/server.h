#include <enet/enet.h>
#include <stdio.h>
#include <cstdint>

#include "network/network.h"

ENetHost *server;

struct Player {
    ENetPeer* peer;
    uint32_t id;
    glm::vec3 position;
};

std::vector<Player> players;
uint32_t next_player_id = 1;

void broadcast_player_position_update(const Player& player);