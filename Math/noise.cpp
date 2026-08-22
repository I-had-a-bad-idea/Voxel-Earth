#include "noise.h"


Noise::Noise(int seed, float frequency, int fractal_octaves, float fractal_lacunarity, float fractal_gain) {
    noise.SetSeed(seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(frequency);
    
    noise.SetFractalOctaves(fractal_octaves);
    noise.SetFractalLacunarity(fractal_lacunarity);
    noise.SetFractalGain(fractal_gain);
}

float Noise::at(float x, float y) {
    return noise.GetNoise(x, y);
}
