#include <enet/enet.h>
#include <glm/vec3.hpp>

#include <stdio.h>
#include <cstdint>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>

#include "network/network.h"
#include "network/world_state.h"

ENetHost *server;

struct Player {
    ENetPeer* peer;
    uint32_t id;
    glm::vec3 position;
};

std::vector<Player> players;
uint32_t next_player_id = 1;

ServerWorldState world_state;

const std::string world_state_filename = "world_state.dat";

void broadcast_player_position_update(const Player& player);
void broadcast_block_edit(int x, int y, int z, BlockType block, uint32_t player_id);
void broadcast_player_disconnected(const uint32_t player_id);

void save_world_state_to_file(const std::string& filename);
void load_world_state_from_file(const std::string& filename);