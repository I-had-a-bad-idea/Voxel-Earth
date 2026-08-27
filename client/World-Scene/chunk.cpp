#include "chunk.h"
#include <stdlib.h>     //for using the function sleep

Chunk::Chunk() 
        : blocks(CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y, BlockType::Air),
            column_tops(CHUNK_SIZE_X * CHUNK_SIZE_Z, 0)
{
    chunk_x = 0;
    chunk_z = 0;
}

Chunk::Chunk(Noise& continental_noise, Noise& detail_noise, Noise& temperature_noise, Noise& moisture_noise, int chunk_x_, int chunk_z_)
    : chunk_x(chunk_x_), chunk_z(chunk_z_),
    blocks(CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y, BlockType::Air), column_tops(CHUNK_SIZE_X * CHUNK_SIZE_Z, 0)
{
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        int world_x = chunk_x * CHUNK_SIZE_X + x;
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int world_z = chunk_z * CHUNK_SIZE_Z + z;

            // TEMPERATURE / MOISTURE
            float temperature = temperature_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));
            float moisture = moisture_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));

            // Convert -1..1 -> 0..1
            temperature = (temperature + 1.0f) * 0.5f;
            moisture = (moisture + 1.0f) * 0.5f;

            // MULTI-SCALE TERRAIN NOISE
            float continental = continental_noise.at(static_cast<float>(world_x) * 0.002f, static_cast<float>(world_z) * 0.002f);
            float hills = continental_noise.at(static_cast<float>(world_x) * 0.008f, static_cast<float>(world_z) * 0.008f);
            float detail = detail_noise.at(static_cast<float>(world_x) * 0.03f, static_cast<float>(world_z) * 0.03f);

            // Convert -1..1 -> 0..1
            continental = (continental + 1.0f) * 0.5f;
            hills = (hills + 1.0f) * 0.5f;
            detail = (detail + 1.0f) * 0.5f;

            // BASE TERRAIN
            float terrain = continental * 0.60f + hills * 0.25f + detail * 0.15f;

            // CONTINENTAL REGIONS
            float coast_factor = 0.0f;
            float highland_factor = 0.0f;
            float mountain_factor = 0.0f;

            // Coast: transition between ocean and land.
            if (continental >= 0.38f && continental < 0.50f) {
                float t = (continental - 0.38f) / 0.12f;
                coast_factor = t * t * (3.0f - 2.0f * t);
            }

            // Highlands begin gradually around 0.62.
            if (continental > 0.58f) {
                float t = (continental - 0.58f) / 0.20f;
                t = std::clamp(t, 0.0f, 1.0f);
                highland_factor = t * t * (3.0f - 2.0f * t);
            }

            // Mountains gradually become more likely deeper into the continent.
            if (continental > 0.68f) {
                float t = (continental - 0.68f) / 0.32f;
                t = std::clamp(t, 0.0f, 1.0f);
                mountain_factor = t * t * (3.0f - 2.0f * t);
            }

            // HEIGHT
            const float sea_level = static_cast<float>(SEA_LEVEL);
            float height_f = sea_level + (terrain - 0.45f) * (CHUNK_SIZE_Y * 0.55f);

            // Highlands add broad elevation.
            height_f += highland_factor * (CHUNK_SIZE_Y * 0.12f);

            // Mountains add much more elevation, but the influence increases smoothly.
            height_f += mountain_factor * mountain_factor * (CHUNK_SIZE_Y * 0.35f);

            // Add additional mountain variation.
            if (mountain_factor > 0.0f) {
                float mountain_detail = hills * 0.7f + detail * 0.3f;
                height_f += mountain_factor * mountain_detail * (CHUNK_SIZE_Y * 0.18f);
            }

            // Ocean floor.
            if (continental < 0.42f) {
                float depth = (0.42f - continental) / 0.42f;
                height_f = sea_level - depth * (CHUNK_SIZE_Y * 0.20f);
            }

            // Coast gets a relatively shallow transition.
            if (coast_factor > 0.0f) {
                height_f = height_f * (1.0f - coast_factor * 0.35f) + sea_level * (coast_factor * 0.35f);
            }

            int height = static_cast<int>(height_f);
            height = std::clamp(height, 1, CHUNK_SIZE_Y - 1);

            // BIOME
            Biome biome;

            if (mountain_factor > 0.55f) {
                biome = Biome::Mountains;
            }
            else if (temperature < 0.30f) {
                biome = Biome::Tundra;
            }
            else if (temperature > 0.70f && moisture < 0.35f) {
                biome = Biome::Desert;
            }
            else if (moisture > 0.65f) {
                biome = Biome::Forest;
            }
            else {
                biome = Biome::Plains;
            }

            // OCEAN
            if (height < SEA_LEVEL) {
                for (int y = 0; y <= height; y++) {
                    if (y < height - 3)
                        set_block(x, y, z, BlockType::Stone);
                    else
                        set_block(x, y, z, BlockType::Sand);
                }

                for (int y = height + 1; y <= SEA_LEVEL; y++) {
                    set_block(x, y, z, BlockType::Water);
                }

                column_tops[x + CHUNK_SIZE_X * z] = SEA_LEVEL;
                continue;
            }

            // BEACH
            const bool beach = height <= SEA_LEVEL + 2;

            // SURFACE BLOCK
            BlockType surface = BlockType::Grass;

            if (beach) {
                surface = BlockType::Sand;
            }
            else if (biome == Biome::Desert) {
                surface = BlockType::Sand;
            }
            else if (biome == Biome::Tundra) {
                surface = BlockType::Snow;
            }
            else if (biome == Biome::Mountains) {
                if (height > CHUNK_SIZE_Y * 0.75f)
                    surface = BlockType::Snow;
                else
                    surface = BlockType::Stone;
            }

            // FILL TERRAIN
            for (int y = 0; y < height; y++) {
                BlockType type = BlockType::Stone;

                if (y >= height - 3 && biome != Biome::Mountains)
                    type = BlockType::Dirt;

                set_block(x, y, z, type);
            }

            // Surface block.
            set_block(x, height, z, surface);

            // DESERT SAND LAYER

            if (biome == Biome::Desert) {
                for (int y = std::max(0, height - 5); y < height; y++) {
                    set_block(x, y, z, BlockType::Sand);
                }
            }

            column_tops[x + CHUNK_SIZE_X * z] = height;
        }
    }
}

MeshData Chunk::generate_mesh_data() {
    MeshData mesh_data;
    mesh_data.vertices.reserve(CHUNK_SIZE_X * CHUNK_SIZE_Z * 16);
    mesh_data.indices.reserve(CHUNK_SIZE_X * CHUNK_SIZE_Z * 24);

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
            const int column_index = x + CHUNK_SIZE_X * z;
            for (int y = 0; y <= column_tops[column_index]; y++) {
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
