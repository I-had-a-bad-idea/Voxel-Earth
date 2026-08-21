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
    Renderer renderer("Voxel Engine", width, height);

    // Create scene
    World world;
    world.setup(renderer);

    bool quit = false;
    // Start renderer loop
    std::cout << "Rendering..." << std::endl;
    while(!quit) {
        renderer.render_scene(world.get_scene());

        for (SDL_Event event; SDL_PollEvent(&event);) {
            // Exit loop if the application is about to close
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
                break;
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