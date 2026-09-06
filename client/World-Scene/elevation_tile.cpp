#include "elevation_tile.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include <httplib/httplib.h>
#include <stdexcept>
#include <vector>

ElevationTile elevation_tile_fetch(int zoom, int tile_x, int tile_y) {
    const std::string path =
        "/elevation-tiles-prod/terrarium/" +
        std::to_string(zoom) + "/" +
        std::to_string(tile_x) + "/" +
        std::to_string(tile_y) + ".png";

    httplib::Client cli("https://s3.amazonaws.com");
    cli.set_follow_location(true);
    cli.set_connection_timeout(15);
    cli.set_read_timeout(15);
    cli.set_write_timeout(15);
    cli.set_default_headers({
        {"User-Agent", "MyWorld/1.0"}
    });

    auto res = cli.Get(path);
    if (!res) {
        throw std::runtime_error("HTTP request failed: " + httplib::to_string(res.error()));
    }
    if (res->status != 200) {
        throw std::runtime_error("HTTP error " + std::to_string(res->status));
    }
    const std::vector<uint8_t> png_data(res->body.begin(), res->body.end());

    int width;
    int height;
    int channels;

    unsigned char* pixels = stbi_load_from_memory(
        png_data.data(),
        static_cast<int>(png_data.size()),
        &width,
        &height,
        &channels,
        3
    );

    if (!pixels) {
        throw std::runtime_error("Failed to decode elevation PNG");
    }

    if (width != 256 || height != 256) {
        stbi_image_free(pixels);

        throw std::runtime_error("Elevation tile isn't 256x256");
    }

    ElevationTile tile;
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const int index = (y * 256 + x) * 3;

            const int r = pixels[index + 0];
            const int g = pixels[index + 1];
            const int b = pixels[index + 2];

            // Mapzen Terrarium format.
            const float elevation = r * 256.0f + g + b / 256.0f - 32768.0f;
            tile.height[y * 256 + x] = elevation;
        }
    }

    stbi_image_free(pixels);
    return tile;
}

GeoCoordinate world_to_geo(int x, int z) {
    const double east_meters = x * METERS_PER_WORLD_BLOCK;
    const double north_meters = -z * METERS_PER_WORLD_BLOCK;
    constexpr double earth_radius = 6378137.0;

    const double latitude = WORLD_ORIGIN_LAT + north_meters / earth_radius * 180.0 / PI;
    const double longitude = WORLD_ORIGIN_LON + east_meters / (earth_radius * std::cos(WORLD_ORIGIN_LAT * PI / 180.0)) * 180.0 / PI;

    return { latitude, longitude };
}

TileCoordinate geo_to_tile(GeoCoordinate coord, int zoom) {
    const double latitude = std::clamp(coord.latitude, -85.05112878, 85.05112878);
    const int n = 1 << zoom;

    const double x = (coord.longitude + 180.0) / 360.0 * n;
    const double lat_rad = latitude * PI / 180.0;
    const double y = (1.0 - std::asinh(std::tan(lat_rad)) / PI) / 2.0 * n;

    return { static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)) };
}