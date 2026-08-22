#include "world.h"

Chunk::Chunk() 
    : blocks(CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y)
{
    chunk_x = 0;
    chunk_z = 0;
}

Chunk::Chunk(Noise& noise, int chunk_x_, int chunk_z_)
    : chunk_x(chunk_x_), chunk_z(chunk_z_),
    blocks(CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y)
{
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int world_x = chunk_x * CHUNK_SIZE_X + x;
            int world_z = chunk_z * CHUNK_SIZE_Z + z;

            const float n = noise.at(static_cast<float>(world_x), static_cast<float>(world_z));
            const float normalized = (n + 1.0f) * 0.5f;

            const int height = static_cast<int>(normalized * static_cast<float>(CHUNK_SIZE_Y / 4));

            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                if (y > height) { // Above is air
                    set_block(x, y, z, Block(BlockType::Air));
                } else if (y == height) {  // top is grass
                    set_block(x, y, z, Block(BlockType::Grass)); 
                } else if (y >= (height - 2)) { // 2 blocks of dirt
                    set_block(x, y, z, Block(BlockType::Dirt));
                } else { // then fill with stone
                    set_block(x, y, z, Block(BlockType::Stone));
                }

            }
        }
    }
}

MeshData Chunk::generate_mesh_data() {
    MeshData mesh_data;

    // Helper to determine whether a block is solid.
    auto is_solid = [&](int x, int y, int z) -> bool {
        // Outside the chunk = air.
        if (x < 0 || x >= CHUNK_SIZE_X ||
            y < 0 || y >= CHUNK_SIZE_Y ||
            z < 0 || z >= CHUNK_SIZE_Z) {
            return false;
        }

        return get_block(x, y, z).block_type != BlockType::Air;
    };

    // Add a quad to the mesh.
    auto add_face = [&](const glm::vec3& position,
                        const glm::vec3& normal,
                        const glm::vec3& v0,
                        const glm::vec3& v1,
                        const glm::vec3& v2,
                        const glm::vec3& v3,
                        AtlasTile tile) {

        uint32_t start_index =
            static_cast<uint32_t>(mesh_data.vertices.size());

        mesh_data.vertices.push_back({
            position + v0,
            normal,
            atlas_uv(tile, glm::vec2(0.0f, 0.0f))
        });

        mesh_data.vertices.push_back({
            position + v1,
            normal,
            atlas_uv(tile, glm::vec2(1.0f, 0.0f))
        });

        mesh_data.vertices.push_back({
            position + v2,
            normal,
            atlas_uv(tile, glm::vec2(1.0f, 1.0f))
        });

        mesh_data.vertices.push_back({
            position + v3,
            normal,
            atlas_uv(tile, glm::vec2(0.0f, 1.0f))
        });

        // Two triangles.
        mesh_data.indices.push_back(start_index + 0);
        mesh_data.indices.push_back(start_index + 1);
        mesh_data.indices.push_back(start_index + 2);

        mesh_data.indices.push_back(start_index + 2);
        mesh_data.indices.push_back(start_index + 3);
        mesh_data.indices.push_back(start_index + 0);
    };

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                const Block& block = get_block(x, y, z);

                if (!is_solid(x, y, z))
                    continue;

                BlockTexture texture = get_block_texture(block.block_type);

                glm::vec3 position(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(z)
                );

                // -X face
                if (!is_solid(x - 1, y, z)) {
                    add_face(
                        position,
                        glm::vec3(-1, 0, 0),

                        glm::vec3(0, 0, 1),
                        glm::vec3(0, 0, 0),
                        glm::vec3(0, 1, 0),
                        glm::vec3(0, 1, 1),

                        texture.side
                    );
                }

                // +X face
                if (!is_solid(x + 1, y, z)) {
                    add_face(
                        position,
                        glm::vec3(1, 0, 0),

                        glm::vec3(1, 0, 0),
                        glm::vec3(1, 0, 1),
                        glm::vec3(1, 1, 1),
                        glm::vec3(1, 1, 0),

                        texture.side
                    );
                }

                // -Y face
                if (!is_solid(x, y - 1, z)) {
                    add_face(
                        position,
                        glm::vec3(0, -1, 0),

                        glm::vec3(0, 0, 0),
                        glm::vec3(1, 0, 0),
                        glm::vec3(1, 0, 1),
                        glm::vec3(0, 0, 1),

                        texture.bottom
                    );
                }

                // +Y face
                if (!is_solid(x, y + 1, z)) {
                    add_face(
                        position,
                        glm::vec3(0, 1, 0),

                        glm::vec3(0, 1, 0),
                        glm::vec3(0, 1, 1),
                        glm::vec3(1, 1, 1),
                        glm::vec3(1, 1, 0),

                        texture.top
                    );
                }

                // -Z face
                if (!is_solid(x, y, z - 1)) {
                    add_face(
                        position,
                        glm::vec3(0, 0, -1),

                        glm::vec3(1, 0, 0),
                        glm::vec3(0, 0, 0),
                        glm::vec3(0, 1, 0),
                        glm::vec3(1, 1, 0),

                        texture.side
                    );
                }

                // +Z face
                if (!is_solid(x, y, z + 1)) {
                    add_face(
                        position,
                        glm::vec3(0, 0, 1),

                        glm::vec3(0, 0, 1),
                        glm::vec3(1, 0, 1),
                        glm::vec3(1, 1, 1),
                        glm::vec3(0, 1, 1),

                        texture.side
                    );
                }
            }
        }
    }

    return mesh_data;
}
