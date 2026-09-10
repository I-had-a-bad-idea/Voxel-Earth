#include "block.h"

BlockTexture get_block_texture(BlockType type)
{
    switch (type) {
        case BlockType::Dirt:
            return {
                { 2, 0 }, // top
                { 2, 0 }, // bottom
                { 2, 0 }  // sides
            };

        case BlockType::Grass:
            return {
                { 0, 0 }, // top
                { 2, 0 }, // bottom
                { 3, 0 }  // sides
            };

        case BlockType::Stone:
            return {
                { 1, 0 }, // top
                { 1, 0 }, // bottom
                { 1, 0 }  // sides
            };

        case BlockType::Air: // doesnt matter for air, since it doesnt get any vertices
            return {
                { 0, 0 }, // top
                { 0, 0 }, // bottom
                { 0, 0 }  // sides
            };
        
        case BlockType::Sand:
            return {
                { 2, 1 }, // top
                { 2, 1 }, // bottom
                { 2, 1 }  // sides
            };
        
        case BlockType::Water:
            return {
                { 14, 0 }, // top
                { 14, 0 }, // bottom
                { 14, 0 }  // sides
            };
        
        case BlockType::Snow:
            return {
                { 2, 4 }, // top
                { 2, 4 }, // bottom
                { 2, 4 }  // sides
            }; 

        case BlockType::Gravel:
            return {
                { 3, 1 }, // top
                { 3, 1 }, // bottom
                { 3, 1 }
            };
        
        case BlockType::Wood:
            return {
                { 5, 1 }, // top
                { 5, 1 }, // bottom
                { 4, 1 }  // sides
            };

        case BlockType::Leaves:
            return {
                { 4, 8 }, // top
                { 4, 8 }, // bottom
                { 4, 8 }  // sides
            };
    }

    return {};
}