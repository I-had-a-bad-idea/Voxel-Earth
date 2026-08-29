#define SDL_MAIN_HANDLED

#include <enet/enet.h>
#include <SDL3/SDL.h>
#include <VGL/renderer.h>
#include <stdio.h>
#include "World-Scene/world.h"

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

glm::vec3 vector_collides_with_block(World& world, const glm::vec3& start, const glm::vec3& vector) {

    auto collides = [&](const glm::vec3& feet) {
        const int min_x = static_cast<int>(std::floor(feet.x - player_half_width));
        const int max_x = static_cast<int>(std::floor(feet.x + player_half_width - overlap_epsilon));
        const int min_y = static_cast<int>(std::floor(feet.y));
        const int max_y = static_cast<int>(std::floor(feet.y + player_height - overlap_epsilon));
        const int min_z = static_cast<int>(std::floor(feet.z - player_half_width));
        const int max_z = static_cast<int>(std::floor(feet.z + player_half_width - overlap_epsilon));

        for (int x = min_x; x <= max_x; ++x) {
            for (int y = min_y; y <= max_y; ++y) {
                for (int z = min_z; z <= max_z; ++z) {
                    if (world.get_block(x, y, z) != BlockType::Air)
                        return true;
                }
            }
        }
        return false;
    };

    glm::vec3 position = start;
    for (int axis = 0; axis < 3; ++axis) {
        const float distance = vector[axis];
        const int steps = static_cast<int>(std::ceil(std::abs(distance) / step_size));
        const float increment = steps > 0 ? distance / static_cast<float>(steps) : 0.0f;

        for (int step = 0; step < steps; ++step) {
            glm::vec3 candidate = position;
            candidate[axis] += increment;
            if (collides(candidate)) {
                if (axis == 1 && distance < 0.0f)
                    position.y = std::floor(position.y);
                break;
            }
            position = candidate;
        }
    }

    return position - start;
}

struct BlockHit {
    glm::ivec3 block;
    glm::ivec3 place_block;
    bool found;
};


BlockHit get_block_looked_at(World& world, const glm::vec3& look_direction) {
    glm::vec3 position = world.get_scene().cam_pos;
    glm::vec3 previous_block(-1);

    for (float distance = 0.0f; distance <= max_block_look_distance; distance += step_size) {
        glm::vec3 candidate = position + look_direction * distance;
        int block_x = static_cast<int>(std::floor(candidate.x));
        int block_y = static_cast<int>(std::floor(candidate.y));
        int block_z = static_cast<int>(std::floor(candidate.z));

        if (world.get_block(block_x, block_y, block_z) != BlockType::Air) {
            return {glm::vec3(block_x, block_y, block_z), previous_block, true};
        }
        previous_block = glm::vec3(block_x, block_y, block_z);
    }

    return {glm::vec3(-1.0f), glm::vec3(-1.0f), false}; // No block found
}

int main(void)
{
    // Define window size
    int width = 960;
    int height = 540;

    // Create renderer
    Renderer renderer("Voxel Engine", width, height, true, nullptr, 1);

    std::cout << "Creating world...\n";
    // Create scene
    World world(renderer);
    world.setup();
    Scene& scene = world.get_scene();

    glm::vec3 camera_velocity(0.0f);
    float pitch = 0.0f;

    std::cout << "Starting rendering..." << std::endl;
    uint64_t last_time {SDL_GetTicks()}; // this is only FPS metrics related stuff
    uint64_t fps_update_time {last_time};
    uint32_t frame_count {0};
    bool quit{ false };

    while (!quit) {
        uint64_t now = SDL_GetTicks();
        float elapsed_time {(now - last_time) / 1000.0f};
        last_time = now;

        renderer.render_scene(world.get_scene()); // Render the scene
        ++frame_count;
        // More FPS stuff
        if (now - fps_update_time >= 1000) {
            float fps{ static_cast<float>(frame_count) / ((now - fps_update_time) / 1000.0f) };
            std::cout << "FPS: " << fps << '\n';
            frame_count = 0;
            fps_update_time = now;
        }
        // Update world
        world.update(elapsed_time);
        
        glm::vec3 player_pos = scene.cam_pos;
        player_pos.y -= player_height;

        glm::vec3 player_pos_block_space;
        player_pos_block_space.x = std::floor(player_pos.x);
        player_pos_block_space.y = std::floor(player_pos.y);
        player_pos_block_space.z = std::floor(player_pos.z);

        bool on_ground = world.get_block(player_pos_block_space.x, player_pos_block_space.y - 1, player_pos_block_space.z) != BlockType::Air;

        // Input
        const bool* keys = SDL_GetKeyboardState(nullptr);

        float speed = length(camera_velocity);

        // Friction
        if (on_ground && speed > 0.0f) {
            camera_velocity.y = 0; // no vertical movement

            float new_speed = std::max(0.0f, speed - friction * elapsed_time);
            camera_velocity = (camera_velocity / speed) * new_speed;
        }


        glm::mat4 camera_transform = glm::translate(
            glm::mat4(1.0f),
            scene.cam_pos
        ) * glm::mat4_cast(scene.cam_orientation);
        glm::vec3 camera_view_direction = glm::normalize(glm::vec3(
            camera_transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)
        ));
        glm::vec3 forward = camera_view_direction;
        forward.y = 0; // dont allow upward movement
        forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::vec3(
            camera_transform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
        ));
        
        glm::vec3 movement(0.0f);
        // Forward / backward
        if (keys[SDL_SCANCODE_W])
            movement.z += 1.0f;

        if (keys[SDL_SCANCODE_S])
            movement.z -= 1.0f;

        // Left / right
        if (keys[SDL_SCANCODE_A])
            movement.x -= 1.0f;

        if (keys[SDL_SCANCODE_D])
            movement.x += 1.0f;

        // Normalize so diagonal movement is not faster
        if (glm::length(movement) > 0.0f)
            movement = glm::normalize(movement);
        
        // Apply gravity
        if (!on_ground) {
            camera_velocity.y -= gravity_acceleration * elapsed_time;
        }
        // JUmping
        if (on_ground && keys[SDL_SCANCODE_SPACE]) {
            camera_velocity.y = jump_velocity; // no * elapsed_time, as this is the velocity, not the acceleration
        }
        
        // Apply movement
        glm::vec3 horizontal_vel (camera_velocity.x, 0, camera_velocity.z);
        glm::vec3 wish_direction = forward * movement.z + right * movement.x;
        if (glm::length(wish_direction) > 0.0f) {
            wish_direction = glm::normalize(wish_direction);

            float acceleration = on_ground ? ground_acceleration : air_acceleration; // use correct acceleration

            horizontal_vel += wish_direction * acceleration * elapsed_time;
        }
        // Clamp horizontal speed
        float horizontal_speed = glm::length(horizontal_vel);

        if (horizontal_speed > move_speed) {
            horizontal_vel = (horizontal_vel / horizontal_speed) * move_speed;
        }

        // Put horizontal velocity back
        camera_velocity.x = horizontal_vel.x;
        camera_velocity.z = horizontal_vel.z;


        // Apply velocity
        glm::vec3 desired_movement = camera_velocity * elapsed_time;

        glm::vec3 allowed_movement = vector_collides_with_block(world, player_pos, desired_movement);
        scene.cam_pos += allowed_movement;

        if (keys[SDL_SCANCODE_ESCAPE]) {
            quit = true;
        }

        for (SDL_Event event; SDL_PollEvent(&event);) {
            // Exit loop if the application is about to close
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
                break;
            }

            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                float mouse_x = (float)event.motion.xrel;
                float mouse_y = (float)event.motion.yrel;

                float next_pitch = glm::clamp(
                    pitch - mouse_y * mouse_sensitivity,
                    -glm::radians(89.0f),
                    glm::radians(89.0f)
                );
                float pitch_delta = next_pitch - pitch;
                pitch = next_pitch;

                scene.cam_orientation = glm::normalize(
                    glm::angleAxis(-mouse_x * mouse_sensitivity, glm::vec3(0.0f, 1.0f, 0.0f))
                    * scene.cam_orientation
                );
                glm::vec3 camera_right = scene.cam_orientation * glm::vec3(1.0f, 0.0f, 0.0f);
                scene.cam_orientation = glm::normalize(
                    glm::angleAxis(pitch_delta, camera_right)
                    * scene.cam_orientation
                );

                scene.cam_rot = glm::eulerAngles(scene.cam_orientation);
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                // Placing / Breaking
                BlockHit hit = get_block_looked_at(world, camera_view_direction);

                if (!hit.found) {
                    continue;
                }
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Break block
                    world.set_block(hit.block.x, hit.block.y, hit.block.z, BlockType::Air);
                }
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    // Place block (directly infront of the block looked at)
                    world.set_block(hit.place_block.x, hit.place_block.y, hit.place_block.z, BlockType::Stone);
                    // TODO: Make placed block choosable
                }
                
            }


            // Zooming with the mouse wheel 
            if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                scene.cam_pos += forward * (float)event.wheel.y * move_speed * 0.1f;
            }
        }
    }
    
    if (enet_initialize() != 0)
    {
        puts("Couldn't initialize ENet");
        return 1;
    }

    ENetHost *client =
        enet_host_create(NULL, 1, 2, 0, 0);

    ENetAddress address;
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 7777;

    ENetPeer *peer =
        enet_host_connect(client, &address, 2, 0);

    ENetEvent event;

    if (enet_host_service(client, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        puts("Connected!");

        ENetPacket *packet =
            enet_packet_create(
                "Hello from client!",
                19,
                ENET_PACKET_FLAG_RELIABLE);

        enet_peer_send(peer, 0, packet);
        enet_host_flush(client);
    }
    else
    {
        puts("Connection failed");
        return 1;
    }

    while (1)
    {
        while (enet_host_service(client, &event, 1000) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    printf("Server says: %s\n",
                           (char *)event.packet->data);

                    enet_packet_destroy(event.packet);

                    enet_peer_disconnect(peer, 0);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    puts("Disconnected");
                    enet_host_destroy(client);
                    enet_deinitialize();
                    // return 0;
                    break;

                default:
                    break;
            }
        }
    }
}