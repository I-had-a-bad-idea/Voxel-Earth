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

    load_world_state_from_file(world_state_filename);

    puts("Server running on port 7777");

    ENetEvent event;

    while (1) {
        // Disable server when user wants to exit
        if (GetAsyncKeyState(VK_ESCAPE)) {
            break;
        }

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
                    packet.header.type = PacketType::AssingPlayerId;
                    packet.header.protocol_version = PROTOCOL_VERSION;
                    packet.player_id = player.id;

                    ENetPacket* enet_packet = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);

                    enet_peer_send(event.peer, NetworkChannel::CHANNEL_RELIABLE, enet_packet);
                    send_world_state(event.peer);

                    printf("Player %u connected\n", player.id);
                    break;
                }
                    case ENET_EVENT_TYPE_RECEIVE: {
                        if (event.packet->dataLength >= sizeof(PacketHeader)) {
                            PacketHeader* header = reinterpret_cast<PacketHeader*>(event.packet->data);

                            uint32_t player_id = *(uint32_t *)event.peer->data;

                            if (header->protocol_version != PROTOCOL_VERSION) {
                                printf("Client %u has incompatible protocol version: %u\n", player_id, header->protocol_version);
                                enet_peer_disconnect(event.peer, 0);
                                break;
                            }
                            switch (header->type) {
                                case PacketType::PlayerPosition: {
                                    if (event.packet->dataLength < sizeof(PlayerPositionPacket)) break;

                                    PlayerPositionPacket *packet = (PlayerPositionPacket *)event.packet->data;

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
                                    break;
                                }
                                case PacketType::BlockEdit: {
                                    if (event.packet->dataLength < sizeof(BlockEditPacket)) break;

                                    BlockEditPacket *packet = (BlockEditPacket *)event.packet->data;

                                    uint32_t packet_player_id = packet->player_id;

                                    if (player_id != packet_player_id) {
                                        break; // ignore, someone is trying to do nonsense
                                    }

                                    // Ignore blocks, that are outside valid range of block types
                                    if (packet->block_type < BlockType::Air || packet->block_type > BlockType::Leaves) {
                                        break;
                                    }

                                    world_state.set_block(packet->x, packet->y, packet->z, packet->block_type);
                                    broadcast_block_edit(packet->x, packet->y, packet->z, packet->block_type, player_id);
                                    break;
                                }
                            }
                        }
                        enet_packet_destroy(event.packet);
                        break;
                    }
                    case ENET_EVENT_TYPE_DISCONNECT: {
                        uint32_t player_id = *(uint32_t *)event.peer->data;
                        printf("Player %u disconnected\n", player_id);
                        broadcast_player_disconnected(player_id);
                        break;
                    }
                default:
                    break;
            }
        }
    }

    puts("Shutting down server...");
    enet_host_destroy(server);
    enet_deinitialize();
    save_world_state_to_file(world_state_filename);

    puts("Server shut down.");
    return 0;
}

void broadcast_player_position_update(const Player& player) {
    PlayerPositionPacket packet;

    packet.header.type = PacketType::PlayerPosition;
    packet.header.protocol_version = PROTOCOL_VERSION;
    packet.player_id = player.id;

    packet.x = player.position.x;
    packet.y = player.position.y;
    packet.z = player.position.z;

    ENetPacket* enet_packet = enet_packet_create(&packet, sizeof(packet), 0);

    enet_host_broadcast(server, NetworkChannel::CHANNEL_MOVEMENT, enet_packet);
}

void broadcast_block_edit(int x, int y, int z, BlockType block, uint32_t player_id) {
    BlockEditPacket packet;

    packet.header.type = PacketType::BlockEdit;
    packet.header.protocol_version = PROTOCOL_VERSION;
    packet.player_id = player_id;
    
    packet.x = x;
    packet.y = y;
    packet.z = z;
    packet.block_type = block;

    ENetPacket* enet_packet = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);

    enet_host_broadcast(server, NetworkChannel::CHANNEL_RELIABLE, enet_packet);
}

void send_world_state(ENetPeer* peer) {
    for (const auto& [position, block_type] : world_state.blocks) {
        BlockEditPacket packet;
        packet.header.type = PacketType::BlockEdit;
        packet.header.protocol_version = PROTOCOL_VERSION;
        packet.player_id = UINT32_MAX;
        packet.x = position.x;
        packet.y = position.y;
        packet.z = position.z;
        packet.block_type = block_type;

        ENetPacket* enet_packet = enet_packet_create(
            &packet,
            sizeof(packet),
            ENET_PACKET_FLAG_RELIABLE
        );
        enet_peer_send(peer, NetworkChannel::CHANNEL_RELIABLE, enet_packet);
    }
}

void broadcast_player_disconnected(const uint32_t player_id) {
    PlayerDisconnectedPacket packet;

    packet.header.type = PacketType::PlayerDisconnected;
    packet.header.protocol_version = PROTOCOL_VERSION;
    packet.player_id = player_id;
    
    ENetPacket* enet_packet = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);

    enet_host_broadcast(server, NetworkChannel::CHANNEL_RELIABLE, enet_packet);
}

void load_world_state_from_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return;
    }
    // Read the number of blocks
    uint32_t num_blocks;
    file.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
    // Read each block's position and type
    for (uint32_t i = 0; i < num_blocks; ++i) {
        WorldBlockPosition position;
        BlockType block_type;
        file.read(reinterpret_cast<char*>(&position.x), sizeof(position.x));
        file.read(reinterpret_cast<char*>(&position.y), sizeof(position.y));
        file.read(reinterpret_cast<char*>(&position.z), sizeof(position.z));
        file.read(reinterpret_cast<char*>(&block_type), sizeof(block_type));
        world_state.blocks[position] = block_type;
    }
    file.close();
    std::cout << "World state loaded from " << filename << std::endl;
}

void save_world_state_to_file(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }
    // Write the number of blocks
    uint32_t num_blocks = static_cast<uint32_t>(world_state.blocks.size());
    file.write(reinterpret_cast<const char*>(&num_blocks), sizeof(num_blocks));
    // Write each block's position and type
    for (const auto& [position, block_type] : world_state.blocks) {
        file.write(reinterpret_cast<const char*>(&position.x), sizeof(position.x));
        file.write(reinterpret_cast<const char*>(&position.y), sizeof(position.y));
        file.write(reinterpret_cast<const char*>(&position.z), sizeof(position.z));
        file.write(reinterpret_cast<const char*>(&block_type), sizeof(block_type));
    }
    file.close();
    std::cout << "World state saved to " << filename << std::endl;
}

