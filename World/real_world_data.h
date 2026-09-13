#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <algorithm>
#include <cmath>
#include <vector>

#include "land_cover.h"

#include <curl/curl.h>

constexpr double METERS_PER_WORLD_BLOCK = 1;
// Grand canyon
constexpr double WORLD_ORIGIN_LAT = 36.1069;
constexpr double WORLD_ORIGIN_LON = -112.1129;
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


struct WorldCoverTile {
    static constexpr int SIZE = 10;

    std::vector<uint8_t> land_cover;

    WorldCoverTile() : land_cover(SIZE * SIZE , 0) {}

    uint8_t get(int x, int y) const {
        return land_cover[y * SIZE + x];
    }
    LandCover get_land_cover(int x, int y) const {
        return static_cast<LandCover>(get(x, y));
    }
} ;

GeoCoordinate world_to_geo(int x, int z);
TileCoordinate geo_to_elevation_tile(GeoCoordinate coord, int zoom);
TileCoordinate geo_to_elevation_tile_pixel(GeoCoordinate coord, int zoom);

class ElevationTileFetcher {
    public:
        ElevationTileFetcher();
        ~ElevationTileFetcher();
        ElevationTile elevation_tile_fetch(int zoom, int tile_x, int tile_y);
    
    private:
        CURL* curl;

        static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};

class WorldCoverFetcher {
    public:
        WorldCoverFetcher();
        ~WorldCoverFetcher();
        WorldCoverTile world_cover_tile_fetch(int tile_lat, int tile_lon);
    
    private:
        CURL* curl;

        static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};
