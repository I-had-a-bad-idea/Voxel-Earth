#include "elevation_tile.h"

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
        throw std::runtime_error(
            "HTTP request failed: " + httplib::to_string(res.error())
        );
    }

    if (res->status != 200) {
        throw std::runtime_error(
            "HTTP error " + std::to_string(res->status)
        );
    }

    const std::vector<uint8_t> png_data(
        res->body.begin(),
        res->body.end()
    );

    // Decode png_data here...
}