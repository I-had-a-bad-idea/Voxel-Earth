#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct ElevationTile {
    static constexpr int SIZE = 256;

    // Elevation in metres.
    std::vector<float> height;

    ElevationTile() : height(SIZE * SIZE, 0.0f) {}

    float get(int x, int y) const {
        return height[y * SIZE + x];
    }
};

class ElevationFetcher {
public:
    ElevationTile fetch(int zoom, int tile_x, int tile_y);
};