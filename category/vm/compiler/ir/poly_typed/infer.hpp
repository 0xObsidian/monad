#pragma once

#include <category/vm/compiler/ir/poly_typed/unify.hpp>

namespace monad::vm::compiler::poly_typed
{
    std::vector<Block> infer_types(
        std::unordered_map<byte_offset, block_id> const &jumpdests,
        std::vector<local_stacks::Block> &&pre_blocks);
}
