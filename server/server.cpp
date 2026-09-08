#include "server.h"

int main(void) {
    if (enet_initialize() != 0) {
        puts("Couldn't initialize ENet");
        return 1;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 7777;

    server = enet_host_create(&address, 32, 2, 0, 0);

    if (!server)
    {
        puts("Couldn't create server");
        return 1;
    }

    puts("Server running on port 7777");

    ENetEvent event;

    while (1) {
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    Player player;
                    player.peer = event.peer;
                    player.id = next_player_id++;
                    player.position = glm::vec3(0.0f);

                    event.peer->data = new uint32_t(player.id);
                    players.push_back(player);

                    AssignPlayerIdPacket packet;
                    packet.type = PacketType::AssingPlayerIdPacket;
                    packet.player_id = player.id;

                    ENetPacket* enet_packet = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);

                    enet_peer_send(event.peer, NetworkChannel::CHANNEL_RELIABLE, enet_packet);

                    printf("Player %u connected\n", player.id);
                    break;
                }
                    case ENET_EVENT_TYPE_RECEIVE: {
                        if (event.packet->dataLength >= sizeof(PlayerPositionPacket)) {
                            PacketType type = *(PacketType *)event.packet->data;

                            if (type == PacketType::PlayerPosition) {
                                PlayerPositionPacket *packet = (PlayerPositionPacket *)event.packet->data;

                                uint32_t player_id = *(uint32_t *)event.peer->data;
                                uint32_t packet_player_id = packet->player_id;

                                if (player_id != packet_player_id) {
                                    break; // ignore, someone is trying to do nonsense
                                }
                                // Update player positon
                                for (Player& player : players) {
                                    if (player.id == player_id) {
                                        player.position = glm::vec3(packet->x, packet->y, packet->z);
                                        
                                        broadcast_player_position_update(player); // tell clients to update this player position
                                        break;
                                    }
                                }
                            }
                        }

                        enet_packet_destroy(event.packet);
                        break;
                    }
                    case ENET_EVENT_TYPE_DISCONNECT: {
                        puts("Client disconnected");
                        break;
                    }
                default:
                    break;
            }
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
}

void broadcast_player_position_update(const Player& player) {
    PlayerPositionPacket packet;

    packet.type = PacketType::PlayerPosition;
    packet.player_id = player.id;

    packet.x = player.position.x;
    packet.y = player.position.y;
    packet.z = player.position.z;

    ENetPacket* enet_packet = enet_packet_create(&packet, sizeof(packet), 0);

    enet_host_broadcast(server, NetworkChannel::CHANNEL_MOVEMENT, enet_packet);
}