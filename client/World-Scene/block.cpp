#include "world.h"

Block::Block() {
    block_type = BlockType_Air;
}

Block::Block(BlockType block_type_) {
    block_type = block_type_;
}