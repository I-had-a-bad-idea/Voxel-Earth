#include "noise.h"


Noise::Noise(int seed, float frequency) {
    noise.SetSeed(seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(frequency);
}

float Noise::at(float x, float y) {
    return noise.GetNoise(x, y);
}
