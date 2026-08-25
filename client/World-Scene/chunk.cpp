#include "world.h"

Chunk::Chunk() 
    : blocks(CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y, BlockType::Air)
{
    chunk_x = 0;
    chunk_z = 0;
}

Chunk::Chunk(Noise& height_noise, Noise& detail_noise, Noise& temperature_noise, Noise& moisture_noise, int chunk_x_, int chunk_z_)
    : chunk_x(chunk_x_), chunk_z(chunk_z_),
    blocks(CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y, BlockType::Air)
{
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        int world_x = chunk_x * CHUNK_SIZE_X + x;
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int world_z = chunk_z * CHUNK_SIZE_Z + z;

            float temperature = temperature_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));
            float moisture = moisture_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));

            temperature = (temperature + 1.0f) * 0.5f;
            moisture = (moisture + 1.0f) * 0.5f;

            float large = height_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));
            float detail = detail_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));

            // Convert -1..1 to 0..1
            large = (large + 1.0f) * 0.5f;
            detail = (detail + 1.0f) * 0.5f;

            // Large-scale terrain
            float terrain = large * 0.75f + detail * 0.25f;

            // Make mountains sharper
            if (terrain > 0.65f) {
                float mountain = (terrain - 0.65f) / 0.35f;
                terrain += mountain * mountain * 0.4f;
            }

            int height = static_cast<int>(terrain * (CHUNK_SIZE_Y * 0.65f));
            height = std::clamp(height,1, CHUNK_SIZE_Y - 1);
            

            Biome biome;
            if (height > CHUNK_SIZE_Y * 0.55f) {
                biome = Biome::Mountains;
            }
            else if (temperature < 0.3f) {
                biome = Biome::Tundra;
            }
            else if (temperature > 0.7f && moisture < 0.35f) {
                biome = Biome::Desert;
            }
            else if (moisture > 0.65f) {
                biome = Biome::Forest;
            }
            else {
                biome = Biome::Plains;
            }

            // oceans and beaches
            if (height < SEA_LEVEL) {
                for (int y = 0; y <= height; y++) {
                    if (y < height - 3)
                        set_block(x, y, z, BlockType::Stone); // fill with stone
                    else
                        set_block(x, y, z, BlockType::Sand); // sand
                }
                for (int y = height + 1; y <= SEA_LEVEL; y++) {
                    set_block(x, y, z, BlockType::Water); // fill lowlands with water
                }
                continue;
            }
            const bool beach = height <= SEA_LEVEL + 2;

            // calculate surface
            BlockType surface = BlockType::Grass;
            if (beach) {
                surface = BlockType::Sand;
            } else if (biome == Biome::Desert) {
                surface = BlockType::Sand;
            } else if (biome == Biome::Tundra) {
                surface = BlockType::Snow;
            }
            

            // fill world with stone and dirt
            for (int y = 0; y < height; y++) {
                BlockType type = BlockType::Stone;
                if (y >= height - 3 && !(biome == Biome::Mountains)) { // mountains are just stone
                    type = BlockType::Dirt;
                }
                set_block(x, y, z, type);
            }
            // set surface
            set_block(x, height, z, surface);
            
            // Fill deserts with sand
            if (biome == Biome::Desert) {
                for (int y = std::max(0, height - 5); y < height; y++) {
                    set_block(x, y, z, BlockType::Sand);
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

        return get_block(x, y, z) != BlockType::Air;
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
                const BlockType& block = get_block(x, y, z);

                if (!is_solid(x, y, z))
                    continue;

                BlockTexture texture = get_block_texture(block);

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
