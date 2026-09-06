#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <algorithm>
#include <cmath>
#include <vector>

constexpr double METERS_PER_WORLD_BLOCK = 1.0;
constexpr double WORLD_ORIGIN_LAT = 0.0;
constexpr double WORLD_ORIGIN_LON = 0.0;
constexpr float PI = 3.1415926535897932384626433832795028841971; // No, I did not look it up (if I made a mistake it is now a feature)

struct GeoCoordinate {
    double latitude;
    double longitude;
};

struct TileCoordinate {
    int x;
    int y;

    bool operator==(const TileCoordinate& other) const {
        return x == other.x && y == other.y;
    }
};

struct TileCoordinateHash {
    std::size_t operator()(const TileCoordinate& coord) const {
        return std::hash<int>()(coord.x) ^ (std::hash<int>()(coord.y) << 1);
    }
};

struct ElevationTile {
    static constexpr int SIZE = 256;

    // Elevation in metres.
    std::vector<float> height;

    ElevationTile() : height(SIZE * SIZE, 0.0f) {}

    float get(int x, int y) const {
        return height[y * SIZE + x];
    }
};

ElevationTile elevation_tile_fetch(int zoom, int tile_x, int tile_y);
GeoCoordinate world_to_geo(int x, int z);
TileCoordinate geo_to_tile(GeoCoordinate coord, int zoom);