#pragma once

#include <VGL/object.h>

enum class BlockType {
    Air,
    Stone,
    Dirt,
    Grass,
    Sand,
    Water,
    Snow,
    Gravel,
    Wood,
    Leaves,
};

struct AtlasTile { // (0, 0) is top left
    uint32_t x;
    uint32_t y;
};

struct BlockTexture {
    AtlasTile top;
    AtlasTile bottom;
    AtlasTile side;
};

BlockTexture get_block_texture(BlockType type);
constexpr float ATLAS_WIDTH = 16.0f;
constexpr float ATLAS_HEIGHT = 20.0f;

glm::vec2 atlas_uv(AtlasTile, glm::vec2 uv);