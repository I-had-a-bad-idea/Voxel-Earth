#ifndef NOISE_H
#define NOISE_H

#include "FastNoiseLite.h"

class Noise {
    public:
        Noise(int seed, float frequency, int fractal_octaves, float fractal_lacunarity, float fractal_gain, FastNoiseLite::FractalType fractal_type);
        float at(float x, float y);
        // TODO: add a lot more configuration possibilities


    private:
        FastNoiseLite noise;
};

#endif