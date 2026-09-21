#pragma once

#include <cstdint>

enum class Biome : char {
    Plains,
    Desert,
    Forest,
    Tundra,
    Mountains,
};


// See https://esa-worldcover.s3.eu-central-1.amazonaws.com/v100/2020/docs/WorldCover_PUM_V1.0.pdf#%5B%7B%22num%22%3A33%2C%22gen%22%3A0%7D%2C%7B%22name%22%3A%22XYZ%22%7D%2C70%2C770%2C0%5D
// table 2
enum class LandCover : uint8_t {
    NoData = 0,
    TreeCover = 10,
    Shrubland = 20,
    Grassland = 30,
    Cropland = 40,
    BuiltUp = 50,
    BareSparseVegetation = 60,
    SnowIce = 70,
    PermanentWater = 80,
    HerbaceousWetland = 90,
    Mangroves = 95,
    MoosLichen = 100,
};