#include <enet/enet.h>
#include <stdio.h>

int main(void)
{
    if (enet_initialize() != 0)
    {
        puts("Couldn't initialize ENet");
        return 1;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 7777;

    ENetHost *server =
        enet_host_create(&address, 32, 2, 0, 0);

    if (!server)
    {
        puts("Couldn't create server");
        return 1;
    }

    puts("Server running on port 7777");

    ENetEvent event;

    while (1)
    {
        while (enet_host_service(server, &event, 1000) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                    puts("Client connected!");
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    printf("Received: %s\n",
                           (char *)event.packet->data);

                    ENetPacket *reply =
                        enet_packet_create(
                            "Hello from server!",
                            19,
                            ENET_PACKET_FLAG_RELIABLE);

                    enet_peer_send(event.peer, 0, reply);

                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    puts("Client disconnected");
                    break;

                default:
                    break;
            }
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
}