#define SDL_MAIN_HANDLED

#include <enet/enet.h>
#include <SDL3/SDL.h>
#include <VGL/renderer.h>
#include <stdio.h>
#include "World-Scene/world.h"

int main(void)
{
    // Define window size
    int width = 960;
    int height = 540;

    // Create renderer
    Renderer renderer("Voxel Engine", width, height, true);

    std::cout << "Creating world...\n";
    // Create scene
    World world(renderer);
    world.setup();
    Scene& scene = world.get_scene();

    glm::vec3 camera_velocity(0.0f);
    float move_speed = 15.0f;
    float mouse_sensitivity = 0.0025f;
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
        
        // Input
        const bool* keys = SDL_GetKeyboardState(nullptr);

        camera_velocity = glm::vec3(0.0f);
        glm::mat4 camera_transform = glm::translate(
            glm::mat4(1.0f),
            scene.cam_pos
        ) * glm::mat4_cast(scene.cam_orientation);
        glm::vec3 forward = glm::normalize(glm::vec3(
            camera_transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)
        ));
        glm::vec3 right = glm::normalize(glm::vec3(
            camera_transform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)
        ));
        
        // Forward / backward
        if (keys[SDL_SCANCODE_W])
            camera_velocity.z += 1.0f;

        if (keys[SDL_SCANCODE_S])
            camera_velocity.z -= 1.0f;

        // Left / right
        if (keys[SDL_SCANCODE_A])
            camera_velocity.x -= 1.0f;

        if (keys[SDL_SCANCODE_D])
            camera_velocity.x += 1.0f;

        // Normalize so diagonal movement is not faster
        if (glm::length(camera_velocity) > 0.0f)
            camera_velocity = glm::normalize(camera_velocity);
    

        // Apply movement
        scene.cam_pos += forward * camera_velocity.z * move_speed * elapsed_time;
        scene.cam_pos += right * camera_velocity.x * move_speed * elapsed_time;

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
                float mouse_y = -(float)event.motion.yrel;

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