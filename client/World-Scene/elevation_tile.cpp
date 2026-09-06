#include "elevation_tile.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include <httplib/httplib.h>
#include <stdexcept>
#include <vector>

ElevationTile ElevationFetcher::fetch(int zoom, int tile_x, int tile_y) {
    std::vector<uint8_t> png_data;
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