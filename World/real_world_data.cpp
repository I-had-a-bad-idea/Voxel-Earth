#include "real_world_data.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include <stdexcept>
#include <vector>

namespace {
struct TiffMemoryFile {
    const std::vector<uint8_t>& data;
    toff_t position = 0;
};

tmsize_t tiff_read(thandle_t handle, void* buffer, tmsize_t size) {
    auto& file = *static_cast<TiffMemoryFile*>(handle); // Get reference to file (as a tiffmemoryfile)

    const toff_t remaining = file.data.size() - std::min(file.position, static_cast<toff_t>(file.data.size()));
    const tmsize_t count = static_cast<tmsize_t>(std::min(static_cast<toff_t>(size), remaining));
    
    std::memcpy(buffer, file.data.data() + file.position, static_cast<size_t>(count));
    file.position += static_cast<toff_t>(count);

    return count;
}

tmsize_t tiff_write(thandle_t, void*, tmsize_t) {
    return 0;
}

toff_t tiff_seek(thandle_t handle, toff_t offset, int whence) {
    auto& file = *static_cast<TiffMemoryFile*>(handle);
    const toff_t size = static_cast<toff_t>(file.data.size());
    toff_t position = file.position;
    if (whence == SEEK_SET) {
        position = offset;
    } else if (whence == SEEK_CUR) {
        position += offset;
    } else if (whence == SEEK_END) {
        position = size + offset;
    } else {
        return static_cast<toff_t>(-1);
    }

    if (position > size) {
        return static_cast<toff_t>(-1);
    }
    file.position = position;
    return position;
}

int tiff_close(thandle_t) {
    return 0;
}

toff_t tiff_size(thandle_t handle) {
    return static_cast<toff_t>(static_cast<TiffMemoryFile*>(handle)->data.size());
}

int tiff_map(thandle_t, void**, toff_t*) {
    return 0;
}

void tiff_unmap(thandle_t, void*, toff_t) {}
}

ElevationTileFetcher::ElevationTileFetcher() {
    curl = curl_easy_init();

    if (!curl)
        throw std::runtime_error("curl_easy_init failed");
}

ElevationTileFetcher::~ElevationTileFetcher() {
    curl_easy_cleanup(curl);
}

size_t ElevationTileFetcher::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* buffer = static_cast<std::vector<uint8_t>*>(userp);
    const size_t total = size * nmemb;

    const auto* bytes = static_cast<const uint8_t*>(contents);
    buffer->insert( buffer->end(), bytes, bytes + total);

    return total;
}

WorldCoverFetcher::WorldCoverFetcher() {
    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("curl_easy_init failed");
    }
}

WorldCoverFetcher::~WorldCoverFetcher() {
    curl_easy_cleanup(curl);
}

ElevationTile ElevationTileFetcher::elevation_tile_fetch(int zoom, int tile_x, int tile_y) {
    std::vector<uint8_t> png_data;
    const std::string url =
        "https://s3.amazonaws.com/"
        "elevation-tiles-prod/terrarium/" +
        std::to_string(zoom) + "/" +
        std::to_string(tile_x) + "/" +
        std::to_string(tile_y) + ".png";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &png_data);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Voxel-Earth/0.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode result = curl_easy_perform(curl);
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (result != CURLE_OK) {
        throw std::runtime_error(curl_easy_strerror(result));
    }

    if (response_code != 200) {
        throw std::runtime_error("HTTP error " + std::to_string(response_code));
    }

    // Decode PNG
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

TileCoordinate geo_to_elevation_tile(GeoCoordinate coord, int zoom) {
    const double latitude = std::clamp(coord.latitude, -85.05112878, 85.05112878);
    const int n = 1 << zoom;

    const double world_x = (coord.longitude + 180.0) / 360.0 * n;
    const double wrapped_x = std::fmod(world_x, static_cast<double>(n));
    const double x = wrapped_x < 0.0 ? wrapped_x + n : wrapped_x;
    const double lat_rad = latitude * PI / 180.0;
    const double y = (1.0 - std::asinh(std::tan(lat_rad)) / PI) / 2.0 * n;

    return { static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)) };
}

TileCoordinate geo_to_elevation_tile_pixel(GeoCoordinate coord, int zoom) {
    const double latitude = std::clamp(coord.latitude, -85.05112878, 85.05112878);
    const int n = 1 << zoom;

    const double world_x = (coord.longitude + 180.0) / 360.0 * n;
    const double wrapped_x = std::fmod(world_x, static_cast<double>(n));
    const double x = wrapped_x < 0.0 ? wrapped_x + n : wrapped_x;
    const double lat_rad = latitude * PI / 180.0;
    const double y = (1.0 - std::asinh(std::tan(lat_rad)) / PI) / 2.0 * n;

    const int tile_x = static_cast<int>(std::floor(x));
    const int tile_y = static_cast<int>(std::floor(y));
    const int pixel_x = std::clamp(static_cast<int>((x - tile_x) * ElevationTile::SIZE), 0, ElevationTile::SIZE - 1);
    const int pixel_y = std::clamp(static_cast<int>((y - tile_y) * ElevationTile::SIZE), 0, ElevationTile::SIZE - 1);
    return { pixel_x, pixel_y };
}

static std::string latitude_prefix(int lat) {
    return lat >= 0 ? "N" : "S";
}

static std::string longitude_prefix(int lon) {
    return lon >= 0 ? "E" : "W";
}

std::string make_world_cover_tile_name(int tile_lat, int tile_lon) {
    std::string name;

    name.append(latitude_prefix(tile_lat)); // append the latitude
    name.append(std::to_string(std::abs(tile_lat))); // absolute value, since sign is in the prefix

    name.append(longitude_prefix(tile_lon));
    int abs_tile_lon = std::abs(tile_lon);
    
    if (abs_tile_lon < 100) { // if longitude only has two digits
        name.append("0"); //  make the third a 0
    }
    name.append(std::to_string(std::abs(tile_lon))); // absolute value, since sign is in the prefix
   

    return name;
}

// See https://esa-worldcover.s3.eu-central-1.amazonaws.com/v100/2020/docs/WorldCover_PUM_V1.0.pdf#%5B%7B%22num%22%3A33%2C%22gen%22%3A0%7D%2C%7B%22name%22%3A%22XYZ%22%7D%2C70%2C770%2C0%5D
// at 3.1 (page 11)
WorldCoverTile WorldCoverFetcher::world_cover_tile_fetch(int tile_lat, int tile_lon) {
    const std::string tile = make_world_cover_tile_name(tile_lat, tile_lon);

    const std::string filename =
        std::string("https://esa-worldcover.s3.eu-central-1.amazonaws.com/") +
        "v200/2021/map/" + // I guess needed to get the correct version; see Terrascope with Python example here: https://esa-worldcover.org/en/data-access
        "ESA_WorldCover_10m_2021_v200_" + // this is the 10 m resolution ESA WorldCover // we want the data from 2021 and version v200
        tile +
        "_Map.tif";
}