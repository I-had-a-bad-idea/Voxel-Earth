#include "chunk.h"

#include <algorithm>
#include <stdlib.h>     //for using the function sleep

Chunk::Chunk() 
        : blocks(CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y, BlockType::Air),
            column_tops(CHUNK_SIZE_X * CHUNK_SIZE_Z, 0)
{
    chunk_x = 0;
    chunk_y = 0;
    chunk_z = 0;
}
Chunk::Chunk(Noise& continental_noise, Noise& hill_noise, Noise& mountain_noise,
             Noise& temperature_noise, Noise& moisture_noise, int chunk_x_, int chunk_y_, int chunk_z_)
    : chunk_x(chunk_x_), chunk_y(chunk_y_), chunk_z(chunk_z_),
      blocks(CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y, BlockType::Air),
      column_tops(CHUNK_SIZE_X * CHUNK_SIZE_Z, 0)
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

            // TERRAIN NOISE
            float continental = continental_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));
            float hills = hill_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));
            float mountains = mountain_noise.at(static_cast<float>(world_x), static_cast<float>(world_z));

            // Convert -1..1 -> 0..1
            continental = (continental + 1.0f) * 0.5f;
            hills = (hills + 1.0f) * 0.5f;
            mountains = (mountains + 1.0f) * 0.5f;

            // CONTINENTAL REGIONS
            float coast_factor = 0.0f;
            float highland_factor = 0.0f;
            float mountain_factor = 0.0f;

            // Coast: transition between ocean and land.
            if (continental >= 0.38f && continental < 0.50f) {
                float t = (continental - 0.38f) / 0.12f;
                coast_factor = t * t * (3.0f - 2.0f * t);
            }

            // Highlands begin around 0.55.
            if (continental > 0.55f) {
                float t = std::clamp((continental - 0.55f) / 0.20f, 0.0f, 1.0f);
                highland_factor = t * t * (3.0f - 2.0f * t);
            }

            // Mountain regions begin around 0.62.
            if (continental > 0.62f) {
                float t = std::clamp((continental - 0.62f) / 0.25f, 0.0f, 1.0f);
                mountain_factor = t * t * (3.0f - 2.0f * t);
            }

            // BASE TERRAIN
            float terrain = continental * 0.55f + hills * 0.45f;
            const float sea_level = static_cast<float>(SEA_LEVEL);
            float height_f = sea_level + (terrain - 0.45f) * (CHUNK_SIZE_Y * 0.45f);

            // HIGHLANDS
            height_f += highland_factor * CHUNK_SIZE_Y * 0.12f;

            // MOUNTAINS
            // Ridged noise creates the actual mountain ridges.
            float mountain_shape = mountains * mountains;

            height_f += mountain_shape * mountain_factor * CHUNK_SIZE_Y * 0.55f;

            // Add some smaller variation to mountain slopes.
            if (mountain_factor > 0.0f) {
                height_f += hills * mountain_factor * CHUNK_SIZE_Y * 0.10f;
            }

            // OCEAN FLOOR
            if (continental < 0.42f) {
                float depth = (0.42f - continental) / 0.42f;
                height_f = sea_level - depth * CHUNK_SIZE_Y * 0.20f;
            }

            // COAST
            if (coast_factor > 0.0f) {
                height_f = glm::mix(height_f, sea_level, coast_factor * 0.35f);
            }

            int height = std::clamp(static_cast<int>(height_f), 1, CHUNK_SIZE_Y - 1);

            // BIOME
            Biome biome;

            if (mountain_factor > 0.45f) {
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

            // BEACH
            const bool beach = height <= SEA_LEVEL + 2;

            // SURFACE
            BlockType surface = BlockType::Grass;

            if (beach || biome == Biome::Desert) {
                surface = BlockType::Sand;
            }
            else if (biome == Biome::Tundra) {
                surface = BlockType::Snow;
            }
            else if (biome == Biome::Mountains) {
                if (height > CHUNK_SIZE_Y * 0.72f)
                    surface = BlockType::Snow;
                else
                    surface = BlockType::Stone;
            }

            const int world_y_base = chunk_y * CHUNK_SIZE_Y;
            int local_top = -1;

            for (int local_y = 0; local_y < CHUNK_SIZE_Y; ++local_y) {
                const int world_y = world_y_base + local_y;
                BlockType type = BlockType::Air;

                if (world_y <= height) {
                    if (height < SEA_LEVEL) {
                        type = world_y < height - 3 ? BlockType::Stone : BlockType::Sand;
                    }
                    else if (world_y == height) {
                        type = surface;
                    }
                    else if (biome == Biome::Desert && world_y >= height - 5) {
                        type = BlockType::Sand;
                    }
                    else if (world_y >= height - 3 && biome != Biome::Mountains) {
                        type = BlockType::Dirt;
                    }
                    else {
                        type = BlockType::Stone;
                    }
                }
                else if (height < SEA_LEVEL && world_y <= SEA_LEVEL) {
                    type = BlockType::Water;
                }

                if (type != BlockType::Air) {
                    set_block(x, local_y, z, type);
                    local_top = local_y;
                }
            }

            column_tops[x + CHUNK_SIZE_X * z] = static_cast<uint8_t>(std::max(0, local_top));
        }
    }
}

MeshData Chunk::generate_mesh_data() {
    MeshData mesh_data;
    mesh_data.vertices.reserve(CHUNK_SIZE_X * CHUNK_SIZE_Z * 24);
    mesh_data.indices.reserve(CHUNK_SIZE_X * CHUNK_SIZE_Z * 36);

    auto is_solid = [&](int x, int y, int z) -> bool {
        if (x < 0 || x >= CHUNK_SIZE_X ||
            y < 0 || y >= CHUNK_SIZE_Y ||
            z < 0 || z >= CHUNK_SIZE_Z) {
            return false;
        }

        return get_block(x, y, z) != BlockType::Air;
    };

    auto same_tile = [](const AtlasTile& lhs, const AtlasTile& rhs) {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    };

    auto add_face_quad = [&](const glm::vec3& v0,
                             const glm::vec3& v1,
                             const glm::vec3& v2,
                             const glm::vec3& v3,
                             const glm::vec3& normal,
                             AtlasTile tile,
                             const glm::vec2& uv0,
                             const glm::vec2& uv1,
                             const glm::vec2& uv2,
                             const glm::vec2& uv3) {
        uint32_t start_index = static_cast<uint32_t>(mesh_data.vertices.size());

        mesh_data.vertices.push_back({ v0, normal, uv0, { tile.x, tile.y } });
        mesh_data.vertices.push_back({ v1, normal, uv1, { tile.x, tile.y } });
        mesh_data.vertices.push_back({ v2, normal, uv2, { tile.x, tile.y } });
        mesh_data.vertices.push_back({ v3, normal, uv3, { tile.x, tile.y } });

        mesh_data.indices.push_back(start_index + 0);
        mesh_data.indices.push_back(start_index + 1);
        mesh_data.indices.push_back(start_index + 2);

        mesh_data.indices.push_back(start_index + 2);
        mesh_data.indices.push_back(start_index + 3);
        mesh_data.indices.push_back(start_index + 0);
    };

    const AtlasTile invalid_tile { UINT32_MAX, UINT32_MAX };
    std::vector<AtlasTile> mask(CHUNK_SIZE_X * CHUNK_SIZE_Y, invalid_tile);

    auto greedy_merge = [&](int width, int height, auto&& get_cell, auto&& emit_rect) {
        std::fill(mask.begin(), mask.begin() + width * height, invalid_tile);

        for (int v = 0; v < height; ++v) {
            for (int u = 0; u < width; ++u) {
                mask[v * width + u] = get_cell(u, v);
            }
        }

        for (int v = 0; v < height; ++v) {
            for (int u = 0; u < width; ++u) {
                AtlasTile& first_tile = mask[v * width + u];
                if (first_tile.x == UINT32_MAX) {
                    continue;
                }

                AtlasTile tile = first_tile;
                int u0 = u;
                int u1 = u;

                while (u1 + 1 < width) {
                    const int next_index = u1 + 1;
                    AtlasTile& next_tile = mask[v * width + next_index];
                    if (next_tile.x == UINT32_MAX || !same_tile(next_tile, tile)) {
                        break;
                    }
                    u1 = next_index;
                }

                int v1 = v;
                while (v1 + 1 < height) {
                    bool row_matches = true;
                    for (int uu = u0; uu <= u1; ++uu) {
                        AtlasTile& row_tile = mask[(v1 + 1) * width + uu];
                        if (row_tile.x == UINT32_MAX || !same_tile(row_tile, tile)) {
                            row_matches = false;
                            break;
                        }
                    }

                    if (!row_matches) {
                        break;
                    }
                    ++v1;
                }

                for (int yy = v; yy <= v1; ++yy) {
                    for (int xx = u0; xx <= u1; ++xx) {
                        mask[yy * width + xx] = invalid_tile;
                    }
                }

                emit_rect(u0, v, u1, v1, tile);
            }
        }
    };

    auto emit_x_face = [&](int x, bool positive_x, int min_v, int max_v, int min_u, int max_u, AtlasTile tile) {
        const float x_coord = static_cast<float>(positive_x ? x + 1 : x);
        const float v0 = static_cast<float>(min_v);
        const float v1 = static_cast<float>(max_v + 1);
        const float u0 = static_cast<float>(min_u);
        const float u1 = static_cast<float>(max_u + 1);

        const glm::vec2 uv0_0 = { static_cast<float>(min_u), static_cast<float>(min_v) };
        const glm::vec2 uv1_0 = { static_cast<float>(max_u + 1), static_cast<float>(min_v) };
        const glm::vec2 uv2_0 = { static_cast<float>(max_u + 1), static_cast<float>(max_v + 1) };
        const glm::vec2 uv3_0 = { static_cast<float>(min_u), static_cast<float>(max_v + 1) };

        if (positive_x) {
            add_face_quad(
                { x_coord, v0, u0 },
                { x_coord, v0, u1 },
                { x_coord, v1, u1 },
                { x_coord, v1, u0 },
                { 1.0f, 0.0f, 0.0f },
                tile,
                uv0_0,
                uv1_0,
                uv2_0,
                uv3_0
            );
        } else {
            add_face_quad(
                { x_coord, v0, u1 },
                { x_coord, v0, u0 },
                { x_coord, v1, u0 },
                { x_coord, v1, u1 },
                { -1.0f, 0.0f, 0.0f },
                tile,
                uv1_0,
                uv0_0,
                uv3_0,
                uv2_0
            );
        }
    };

    auto emit_y_face = [&](int y, bool positive_y, int min_u, int max_u, int min_v, int max_v, AtlasTile tile) {
        const float y_coord = static_cast<float>(positive_y ? y + 1 : y);
        const float u0 = static_cast<float>(min_u);
        const float u1 = static_cast<float>(max_u + 1);
        const float v0 = static_cast<float>(min_v);
        const float v1 = static_cast<float>(max_v + 1);

        const glm::vec2 uv0_0 = { static_cast<float>(min_u), static_cast<float>(min_v) };
        const glm::vec2 uv1_0 = { static_cast<float>(max_u + 1), static_cast<float>(min_v) };
        const glm::vec2 uv2_0 = { static_cast<float>(max_u + 1), static_cast<float>(max_v + 1) };
        const glm::vec2 uv3_0 = { static_cast<float>(min_u), static_cast<float>(max_v + 1) };

        if (positive_y) {
            add_face_quad(
                { u0, y_coord, v0 },
                { u0, y_coord, v1 },
                { u1, y_coord, v1 },
                { u1, y_coord, v0 },
                { 0.0f, 1.0f, 0.0f },
                tile,
                uv0_0,
                uv3_0,
                uv2_0,
                uv1_0
            );
        } else {
            add_face_quad(
                { u0, y_coord, v0 },
                { u1, y_coord, v0 },
                { u1, y_coord, v1 },
                { u0, y_coord, v1 },
                { 0.0f, -1.0f, 0.0f },
                tile,
                uv0_0,
                uv1_0,
                uv2_0,
                uv3_0
            );
        }
    };

    auto emit_z_face = [&](int z, bool positive_z, int min_u, int max_u, int min_v, int max_v, AtlasTile tile) {
        const float z_coord = static_cast<float>(positive_z ? z + 1 : z);
        const float u0 = static_cast<float>(min_u);
        const float u1 = static_cast<float>(max_u + 1);
        const float v0 = static_cast<float>(min_v);
        const float v1 = static_cast<float>(max_v + 1);

        const glm::vec2 uv0_0 = { static_cast<float>(min_u), static_cast<float>(min_v) };
        const glm::vec2 uv1_0 = { static_cast<float>(max_u + 1), static_cast<float>(min_v) };
        const glm::vec2 uv2_0 = { static_cast<float>(max_u + 1), static_cast<float>(max_v + 1) };
        const glm::vec2 uv3_0 = { static_cast<float>(min_u), static_cast<float>(max_v + 1) };

        if (positive_z) {
            add_face_quad(
                { u0, v0, z_coord },
                { u1, v0, z_coord },
                { u1, v1, z_coord },
                { u0, v1, z_coord },
                { 0.0f, 0.0f, 1.0f },
                tile,
                uv0_0,
                uv1_0,
                uv2_0,
                uv3_0
            );
        } else {
            add_face_quad(
                { u1, v0, z_coord },
                { u0, v0, z_coord },
                { u0, v1, z_coord },
                { u1, v1, z_coord },
                { 0.0f, 0.0f, -1.0f },
                tile,
                uv1_0,
                uv0_0,
                uv3_0,
                uv2_0
            );
        }
    };

    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        greedy_merge(
            CHUNK_SIZE_Z,
            CHUNK_SIZE_Y,
            [&](int z, int y) -> AtlasTile {
                if (!is_solid(x, y, z) || is_solid(x - 1, y, z)) {
                    return { UINT32_MAX, UINT32_MAX };
                }
                return get_block_texture(get_block(x, y, z)).side;
            },
            [&](int z0, int y0, int z1, int y1, AtlasTile tile) {
                emit_x_face(x, false, y0, y1, z0, z1, tile);
            }
        );

        greedy_merge(
            CHUNK_SIZE_Z,
            CHUNK_SIZE_Y,
            [&](int z, int y) -> AtlasTile {
                if (!is_solid(x, y, z) || is_solid(x + 1, y, z)) {
                    return { UINT32_MAX, UINT32_MAX };
                }
                return get_block_texture(get_block(x, y, z)).side;
            },
            [&](int z0, int y0, int z1, int y1, AtlasTile tile) {
                emit_x_face(x, true, y0, y1, z0, z1, tile);
            }
        );
    }

    for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
        greedy_merge(
            CHUNK_SIZE_X,
            CHUNK_SIZE_Z,
            [&](int x, int z) -> AtlasTile {
                if (!is_solid(x, y, z) || is_solid(x, y - 1, z)) {
                    return { UINT32_MAX, UINT32_MAX };
                }
                return get_block_texture(get_block(x, y, z)).bottom;
            },
            [&](int x0, int z0, int x1, int z1, AtlasTile tile) {
                emit_y_face(y, false, x0, x1, z0, z1, tile);
            }
        );

        greedy_merge(
            CHUNK_SIZE_X,
            CHUNK_SIZE_Z,
            [&](int x, int z) -> AtlasTile {
                if (!is_solid(x, y, z) || is_solid(x, y + 1, z)) {
                    return { UINT32_MAX, UINT32_MAX };
                }
                return get_block_texture(get_block(x, y, z)).top;
            },
            [&](int x0, int z0, int x1, int z1, AtlasTile tile) {
                emit_y_face(y, true, x0, x1, z0, z1, tile);
            }
        );
    }

    for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
        greedy_merge(
            CHUNK_SIZE_X,
            CHUNK_SIZE_Y,
            [&](int x, int y) -> AtlasTile {
                if (!is_solid(x, y, z) || is_solid(x, y, z - 1)) {
                    return { UINT32_MAX, UINT32_MAX };
                }
                return get_block_texture(get_block(x, y, z)).side;
            },
            [&](int x0, int y0, int x1, int y1, AtlasTile tile) {
                emit_z_face(z, false, x0, x1, y0, y1, tile);
            }
        );

        greedy_merge(
            CHUNK_SIZE_X,
            CHUNK_SIZE_Y,
            [&](int x, int y) -> AtlasTile {
                if (!is_solid(x, y, z) || is_solid(x, y, z + 1)) {
                    return { UINT32_MAX, UINT32_MAX };
                }
                return get_block_texture(get_block(x, y, z)).side;
            },
            [&](int x0, int y0, int x1, int y1, AtlasTile tile) {
                emit_z_face(z, true, x0, x1, y0, y1, tile);
            }
        );
    }

    return mesh_data;
}