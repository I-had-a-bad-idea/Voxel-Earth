#include "world.h"


Chunk::Chunk(Noise& noise, int chunk_x, int chunk_z)
    : chunk_x(chunk_x), chunk_z(chunk_z)
{
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int height = noise.at((float)x, (float)z);
            
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                if (y < height) {
                    blocks[x][z][y] = Block(BlockType_Grass);
                } else {
                    blocks[x][z][y] = Block(BlockType_Default);
                }
            }
        }
    }
}