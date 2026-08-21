#ifndef NOISE_H
#define NOISE_H

#include "FastNoiseLite.h"

class Noise {
    public:
        Noise(int seed, float frequency);
        float at(int x, int y);
        // TODO: add a lot more configuration possibilities


    private:
        FastNoiseLite noise;
};

#endif