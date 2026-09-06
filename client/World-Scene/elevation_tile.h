#pragma once

#include <array>
#include <cstdint>
#include <string>
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


GeoCoordinate world_to_geo(int x, int z) {
    const double east_meters = x * METERS_PER_WORLD_BLOCK;
    const double north_meters = -z * METERS_PER_WORLD_BLOCK;
    constexpr double earth_radius = 6378137.0;

    const double latitude = WORLD_ORIGIN_LAT + north_meters / earth_radius * 180.0 / PI;
    const double longitude = WORLD_ORIGIN_LON + east_meters / (earth_radius * std::cos(WORLD_ORIGIN_LAT * PI / 180.0)) * 180.0 / PI;

    return { latitude, longitude };
}

TileCoordinate geo_to_tile(GeoCoordinate coord, int zoom) {
    const int latitude = std::clamp(coord.latitude, -85.05112878, 85.05112878);
    const int n = 1 << zoom;

    const double x = (coord.longitude + 180.0) / 360.0 * n;
    const double lat_rad = latitude * PI / 180.0;
    const double y = (1.0 - std::asinh(std::tan(lat_rad)) / PI) / 2.0 * n;

    return { static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)) };
}