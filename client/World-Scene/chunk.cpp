#include "world.h"

Chunk::Chunk() {
    chunk_x = 0;
    chunk_z = 0;
}

Chunk::Chunk(Noise& noise, int chunk_x, int chunk_z)
    : chunk_x(chunk_x), chunk_z(chunk_z)
{
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int world_x = chunk_x * CHUNK_SIZE_X + x;
            int world_z = chunk_z * CHUNK_SIZE_Z + z;

            float height = noise.at((float)world_x, (float)world_z) * 10.0f;
            
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                if (y < height) {
                    blocks[x][z][y] = Block(BlockType_Grass);
                } else {
                    blocks[x][z][y] = Block(BlockType_Air);
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

        return blocks[x][z][y].block_type != BlockType_Air;
    };

    // Add a quad to the mesh.
    auto add_face = [&](const glm::vec3& position,
                        const glm::vec3& normal,
                        const glm::vec3& v0,
                        const glm::vec3& v1,
                        const glm::vec3& v2,
                        const glm::vec3& v3) {

        uint32_t start_index =
            static_cast<uint32_t>(mesh_data.vertices.size());

        mesh_data.vertices.push_back({
            position + v0,
            normal,
            glm::vec2(0.0f, 0.0f)
        });

        mesh_data.vertices.push_back({
            position + v1,
            normal,
            glm::vec2(1.0f, 0.0f)
        });

        mesh_data.vertices.push_back({
            position + v2,
            normal,
            glm::vec2(1.0f, 1.0f)
        });

        mesh_data.vertices.push_back({
            position + v3,
            normal,
            glm::vec2(0.0f, 1.0f)
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

                if (!is_solid(x, y, z))
                    continue;

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
                        glm::vec3(0, 1, 1)
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
                        glm::vec3(1, 1, 0)
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
                        glm::vec3(0, 0, 1)
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
                        glm::vec3(1, 1, 0)
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
                        glm::vec3(1, 1, 0)
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
                        glm::vec3(0, 1, 1)
                    );
                }
            }
        }
    }

    return mesh_data;
}
