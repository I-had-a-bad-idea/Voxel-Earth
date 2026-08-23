#include "world.h"

glm::vec2 atlas_uv(AtlasTile tile, glm::vec2 uv) {
    glm::vec2 tile_size (
        1.0f / ATLAS_WIDTH,
        1.0f / ATLAS_HEIGHT
    );

    glm::vec2 tile_offset (
        static_cast<float>(tile.x),
        static_cast<float>((ATLAS_HEIGHT - 1) - tile.y) // y+ is up, here we flip it  (the -1 is because it starts at 0, but ATLAST_HEIGHT starts counting from 1)
    );

    return tile_offset * tile_size + uv * tile_size;
}

BlockTexture get_block_texture(BlockType type)
{
    switch (type) {
        case BlockType::Dirt:
            return {
                { 2, 0 }, // top
                { 2, 0 }, // bottom
                { 2, 0 }  // sides
            };

        case BlockType::Grass:
            return {
                { 0, 0 }, // top
                { 2, 0 }, // bottom
                { 3, 0 }  // sides
            };

        case BlockType::Stone:
            return {
                { 1, 0 }, // top
                { 1, 0 }, // bottom
                { 1, 0 }  // sides
            };

        case BlockType::Air: // doesnt matter for air, since it doesnt get any vertices
            return {
                { 0, 0 },
                { 0, 0 },
                { 0, 0 }
            };
    }

    return {};
}