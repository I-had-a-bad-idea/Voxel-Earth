#pragma once

#include <VGL/object.h>

static uint32_t pack_pos(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & 0x7Fu)
         | ((y & 0x7Fu) << 7)
         | ((z & 0x7Fu) << 14);
}

static uint32_t pack_uv(uint32_t u, uint32_t v)
{
    return (u & 0xFFFFu)
         | ((v & 0xFFFFu) << 16);
}

static uint32_t pack_atlas_tile(uint32_t x, uint32_t y)
{
    return (x & 0xFFFFu)
         | ((y & 0xFFFFu) << 16);
}

enum class PackedNormal : uint32_t {
    NegX = 0,
    PosX = 1,
    NegY = 2,
    PosY = 3,
    NegZ = 4,
    PosZ = 5
};