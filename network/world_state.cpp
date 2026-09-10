#include "world_state.h"

BlockType ServerWorldState::get_block(int x, int y, int z) const {
    const auto it = blocks.find({x, y, z});
    if (it == blocks.end()) {
        return BlockType::Air;
    }

    return it->second;
}

void ServerWorldState::set_block(int x, int y, int z, BlockType block) {
    const WorldBlockPosition position{x, y, z};
    if (block == BlockType::Air) {
        blocks.erase(position);
        return;
    }

    blocks.insert_or_assign(position, block);
}
