#include <enet/enet.h>
#include <SDL3/SDL.h>
#include <VGL/renderer.h>
#include <stdio.h>

#include "World-Scene/world.h"
#include "network/network.h"

constexpr float max_block_look_distance = 5.0f; // distance at which a block can be looked at / modified
constexpr float move_speed = 15.0f; // blocks/sec
constexpr float ground_acceleration = 80.0f; // blocks / s^2
constexpr float air_acceleration = 20.0f; // blocks / s^2
constexpr float mouse_sensitivity = 0.0025f;
constexpr float gravity_acceleration = 5.0f; // blocks / s^2
constexpr float jump_velocity = 3.0f;
constexpr float friction = 30.0f; // currently a flat value (TODO: make friction block dependent)
constexpr float player_height = 1.0f; 
constexpr float step_size = 0.05f;
constexpr float player_half_width = 0.3f;
constexpr float overlap_epsilon = 0.0001f;

// Define window size
int width = 960;
int height = 540;

glm::vec3 camera_velocity(0.0f);
float pitch = 0.0f;
bool fly {false};

uint64_t last_time {SDL_GetTicks()}; // this is only FPS metrics related stuff
uint64_t fps_update_time {last_time};
uint32_t frame_count {0};
bool quit {false};

uint32_t my_player_id;