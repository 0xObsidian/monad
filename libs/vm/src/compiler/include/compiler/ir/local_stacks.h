#pragma once

#include <compiler/ir/basic_blocks.h>
#include <compiler/ir/bytecode.h>

#include <functional>

namespace monad::compiler::local_stacks
{
    enum class ValueIs
    {
        PARAM_ID,
        COMPUTED,
        LITERAL
    };

    struct Value
    {
        ValueIs is;
        uint256_t data; // unused if COMPUTED
    };

    struct Block
    {
        std::size_t min_params;
        std::vector<Value> output;

        std::vector<bytecode::Instruction> instrs;
        basic_blocks::Terminator terminator;
        block_id fallthrough_dest; // value for JumpI and JumpDest, otherwise
                                   // INVALID_BLOCK_ID
    };

    class LocalStacksIR
    {
    public:
        LocalStacksIR(basic_blocks::BasicBlocksIR const &&ir);
        std::unordered_map<byte_offset, block_id> jumpdests;
        std::vector<Block> blocks;
        uint64_t codesize;

    private:
        Block to_block(basic_blocks::Block const &&block);
    };

}

template <>
struct std::formatter<monad::compiler::local_stacks::Value>
{
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(
        monad::compiler::local_stacks::Value const &val,
        std::format_context &ctx) const
    {
        switch (val.is) {
        case monad::compiler::local_stacks::ValueIs::PARAM_ID:
            return std::format_to(
                ctx.out(), "%p{}", intx::to_string(val.data, 10));
        case monad::compiler::local_stacks::ValueIs::COMPUTED:
            return std::format_to(ctx.out(), "COMPUTED");
        default:
            return std::format_to(ctx.out(), "{}", val.data);
        }
    }
};

template <>
struct std::formatter<monad::compiler::local_stacks::Block>
{
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(
        monad::compiler::local_stacks::Block const &blk,
        std::format_context &ctx) const
    {

        std::format_to(ctx.out(), "    min_params: {}\n", blk.min_params);

        for (auto const &tok : blk.instrs) {
            std::format_to(ctx.out(), "      {}\n", tok);
        }

        std::format_to(ctx.out(), "    {}", blk.terminator);
        if (blk.fallthrough_dest != monad::compiler::INVALID_BLOCK_ID) {
            std::format_to(ctx.out(), " {}", blk.fallthrough_dest);
        }
        std::format_to(ctx.out(), "\n    output: [");
        for (monad::compiler::local_stacks::Value const &val : blk.output) {
            std::format_to(ctx.out(), " {}", val);
        }
        return std::format_to(ctx.out(), " ]\n");
    }
};

template <>
struct std::formatter<monad::compiler::local_stacks::LocalStacksIR>
{
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(
        monad::compiler::local_stacks::LocalStacksIR const &ir,
        std::format_context &ctx) const
    {

        std::format_to(ctx.out(), "local_stacks:\n");
        int i = 0;
        for (auto const &blk : ir.blocks) {
            std::format_to(ctx.out(), "  block {}:\n", i);
            std::format_to(ctx.out(), "{}", blk);
            i++;
        }
        std::format_to(ctx.out(), "\n  jumpdests:\n");
        for (auto const &[k, v] : ir.jumpdests) {
            std::format_to(ctx.out(), "    {}:{}\n", k, v);
        }
        return std::format_to(ctx.out(), "");
    }
};
