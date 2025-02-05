#include "compiler/evm_opcodes.h"
#include "compiler/ir/basic_blocks.h"
#include "compiler/ir/local_stacks.h"
#include "compiler/ir/poly_typed/block.h"
#include "compiler/ir/poly_typed/infer.h"
#include "compiler/ir/poly_typed/kind.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <variant>
#include <vector>

using namespace monad::compiler;
using namespace monad::compiler::poly_typed;

TEST(infer, test_add)
{
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR({ADD}));

    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);
    ASSERT_EQ(blocks.size(), 1);
    ASSERT_TRUE(std::holds_alternative<Stop>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(blocks[0].kind, cont_kind({word, word}, 0)));
}

TEST(infer, test_param_jump)
{
    auto ir = local_stacks::LocalStacksIR(
        basic_blocks::BasicBlocksIR({PUSH1, 1, ADD, SWAP1, JUMP}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);
    ASSERT_EQ(blocks.size(), 1);
    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[0].terminator).jump_kind, cont_kind({word}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[0].kind, cont_kind({word, cont(cont_kind({word}, 0))}, 0)));
}

TEST(infer, test_literal_valid_jump)
{
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR(
        {PUSH1, 8, SWAP1, PUSH1, 1, ADD, SWAP1, JUMP, JUMPDEST, POP, POP}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);

    ASSERT_EQ(blocks.size(), 2);

    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[0].terminator).jump_kind,
        cont_kind({word, kind_var(0)}, 0)));
    ASSERT_TRUE(alpha_equal(blocks[0].kind, cont_kind({word, kind_var(0)}, 0)));

    ASSERT_TRUE(std::holds_alternative<Stop>(blocks[1].terminator));
    ASSERT_TRUE(
        alpha_equal(blocks[1].kind, cont_kind({kind_var(0), kind_var(1)}, 0)));
}

TEST(infer, test_literal_invalid_jump)
{
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR(
        {PUSH1, 0, SWAP1, PUSH1, 1, ADD, SWAP1, JUMP, JUMPDEST, POP, POP}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);

    ASSERT_EQ(blocks.size(), 2);

    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[0].terminator).jump_kind, cont_kind({word}, 0)));
    ASSERT_TRUE(alpha_equal(blocks[0].kind, cont_kind({word}, 0)));

    ASSERT_TRUE(std::holds_alternative<Stop>(blocks[1].terminator));
    ASSERT_TRUE(
        alpha_equal(blocks[1].kind, cont_kind({kind_var(0), kind_var(1)}, 0)));
}

TEST(infer, test_computed_jump)
{
    auto ir = local_stacks::LocalStacksIR(
        basic_blocks::BasicBlocksIR({PUSH1, 1, ADD, JUMP}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);

    ASSERT_EQ(blocks.size(), 1);
    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[0].terminator).jump_kind, cont_words));
    ASSERT_TRUE(alpha_equal(blocks[0].kind, cont_words));
}

TEST(infer, test_return)
{
    auto ir =
        local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR({POP, RETURN}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);
    ASSERT_EQ(blocks.size(), 1);
    ASSERT_TRUE(std::holds_alternative<Return>(blocks[0].terminator));
    ASSERT_TRUE(
        alpha_equal(blocks[0].kind, cont_kind({kind_var(0), word, word}, 0)));
}

TEST(infer, test_param_jumpi)
{
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR(
        {PUSH1, 1, ADD, SWAP1, JUMPI, SELFDESTRUCT}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);

    ASSERT_EQ(blocks.size(), 2);

    ASSERT_TRUE(std::holds_alternative<JumpI>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<JumpI>(blocks[0].terminator).jump_kind, cont_kind({word}, 0)));
    ASSERT_TRUE(alpha_equal(
        std::get<JumpI>(blocks[0].terminator).fallthrough_kind,
        cont_kind({word}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[0].kind,
        cont_kind({word, cont(cont_kind({word}, 0)), word}, 0)));

    ASSERT_TRUE(std::holds_alternative<SelfDestruct>(blocks[1].terminator));
    ASSERT_TRUE(alpha_equal(blocks[1].kind, cont_kind({word}, 0)));
}

TEST(infer, test_literal_valid_jumpi)
{
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR(
        {PUSH1, 4, JUMPI, SELFDESTRUCT, JUMPDEST, POP}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);

    ASSERT_EQ(blocks.size(), 3);

    ASSERT_TRUE(std::holds_alternative<JumpI>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<JumpI>(blocks[0].terminator).jump_kind, cont_kind({word}, 0)));
    ASSERT_TRUE(alpha_equal(
        std::get<JumpI>(blocks[0].terminator).fallthrough_kind,
        cont_kind({word}, 0)));
    ASSERT_TRUE(alpha_equal(blocks[0].kind, cont_kind({word, word}, 0)));

    ASSERT_TRUE(std::holds_alternative<SelfDestruct>(blocks[1].terminator));
    ASSERT_TRUE(alpha_equal(blocks[1].kind, cont_kind({word}, 0)));

    ASSERT_TRUE(std::holds_alternative<Stop>(blocks[2].terminator));
    ASSERT_TRUE(alpha_equal(blocks[2].kind, cont_kind({kind_var(0)}, 0)));
}

TEST(infer, test_literal_var_output)
{
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR(
        {PUSH1,    255,      PUSH1, 14,    SWAP2, PUSH1, 17,       JUMPI,
         JUMPDEST, PUSH1,    1,     ADD,   SWAP1, JUMP,  JUMPDEST, POP,
         STOP,     JUMPDEST, SWAP1, PUSH1, 8,     JUMP}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);

    ASSERT_EQ(blocks.size(), 4);

    ASSERT_TRUE(std::holds_alternative<JumpI>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<JumpI>(blocks[0].terminator).fallthrough_kind,
        cont_kind({word, cont(cont_kind({word}, 0))}, 0)));
    ASSERT_TRUE(alpha_equal(
        std::get<JumpI>(blocks[0].terminator).jump_kind,
        cont_kind({cont(cont_kind({word}, 0)), word}, 0)));
    ASSERT_TRUE(alpha_equal(blocks[0].kind, cont_kind({word}, 0)));
}

TEST(infer, test_sum)
{
    uint8_t const loop = 1;
    uint8_t const ret = 14;
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR(
        {// Word,(Word,s -> Exit),s -> Exit
         DUP1, // Word,Word,(Word,s -> Exit),s -> Exit
               // loop:
         JUMPDEST, // Word,Word,(Word,s -> Exit),s -> Exit
         DUP1, // Word,Word,Word,(Word,s -> Exit),s -> Exit
         ISZERO, // Word,Word,Word,(Word,s -> Exit),s -> Exit
         PUSH1,
         ret, // ret,Word,Word,Word,(Word,s -> Exit),s -> Exit
         JUMPI, // Word,Word,(Word,s -> Exit),s -> Exit
                //
         DUP1, // Word,Word,Word,(Word,s -> Exit),s -> Exit
         SWAP2, // Word,Word,Word,(Word,s -> Exit),s -> Exit
         ADD, // Word,Word,(Word,s -> Exit),s -> Exit
         SWAP1, // Word,Word,(Word,s -> Exit),s -> Exit
         PUSH1,
         loop, // loop,Word,Word,(Word,s -> Exit),s -> Exit
         JUMP, // Word,Word,(Word,s -> Exit),s -> Exit
               // ret:
         JUMPDEST, // a,b,(b,s -> Exit),s -> Exit
         POP,
         SWAP1,
         JUMP}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);

    ASSERT_EQ(blocks.size(), 4);

    ASSERT_TRUE(std::holds_alternative<FallThrough>(blocks[0].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<FallThrough>(blocks[0].terminator).fallthrough_kind,
        cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[0].kind, cont_kind({word, cont(cont_kind({word}, 0))}, 0)));

    ASSERT_TRUE(std::holds_alternative<JumpI>(blocks[1].terminator));
    ASSERT_TRUE(weak_equal(
        std::get<JumpI>(blocks[1].terminator).fallthrough_kind,
        std::get<JumpI>(blocks[1].terminator).jump_kind));
    ASSERT_TRUE(alpha_equal(
        std::get<JumpI>(blocks[1].terminator).fallthrough_kind,
        cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[1].kind,
        cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)));

    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[2].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[2].terminator).jump_kind,
        cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[2].kind,
        cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)));

    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[3].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[3].terminator).jump_kind,
        cont_kind({kind_var(0)}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[3].kind,
        cont_kind(
            {kind_var(0), kind_var(1), cont(cont_kind({kind_var(1)}, 0))}, 0)));
}

TEST(infer, test_fib)
{
    // fib : forall r. Word -> (Word -> r) -> r
    // fib n k = if n < 2 then retk n k else fib (n - 2) fibk n k
    // fibk : forall r. Word -> Word -> (Word -> r) -> r
    // fibk y n k = fib (n - 1) addk y k
    // addk : forall r. Word -> Word -> (Word -> r) -> r
    // addk x y k = k (x + y)
    // retk : forall a r. a -> (Word -> r) -> r
    // retk _ k = k 1
    uint8_t const fib = 0;
    uint8_t const fibk = 17;
    uint8_t const addk = 28;
    uint8_t const retk = 32;
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR({
        // fib:
        JUMPDEST, // Word,(Word,s -> Exit),s -> Exit
        PUSH1,
        2, // Word,Word,(Word,s -> Exit),s -> Exit
        DUP2, // Word,Word,Word,(Word,s -> Exit),s -> Exit
        LT, // Word,Word,(Word,s -> Exit),s -> Exit
        PUSH1,
        retk, // retk,Word,Word,(Word,s -> Exit),s -> Exit
        JUMPI, // Word,(Word,s -> Exit),s -> Exit
        //
        PUSH1,
        fibk, // fibk,Word,(Word,s -> Exit),s -> Exit
        DUP2, // Word,fibk,Word,(Word,s -> Exit),s -> Exit
        PUSH1,
        2, // Word,Word,fibk,Word,(Word,s -> Exit),s -> Exit
        SUB, // Word,fibk,Word,(Word,s -> Exit),s -> Exit
        PUSH1,
        fib, // fib,Word,fibk,Word,(Word,s -> Exit),s -> Exit
        JUMP, // Word,fibk,Word,(Word,s -> Exit),s -> Exit
        // fibk:
        JUMPDEST, // Word,Word,(Word,s -> Exit),s -> Exit
        SWAP1, // Word,Word,(Word,s -> Exit),s -> Exit
        PUSH1,
        1, // Word,Word,Word,(Word,s -> Exit),s -> Exit
        SUB, // Word,Word,(Word,s -> Exit),s -> Exit
        PUSH1,
        addk, // addk,Word,Word,(Word,s -> Exit),s -> Exit
        SWAP1, // Word,addk,Word,(Word,s -> Exit),s -> Exit
        PUSH1,
        fib, // fib,Word,addk,Word,(Word,s -> Exit),s -> Exit
        JUMP, // Word,addk,Word,(Word,s -> Exit),s -> Exit
        // addk:
        JUMPDEST, // Word,Word,(Word,s -> Exit),s -> Exit
        ADD, // Word,(Word,s -> Exit),s -> Exit
        SWAP1, // (Word,s -> Exit),Word,s -> Exit
        JUMP, // Word,s -> Exit
        // retk:
        JUMPDEST, // v,(Word,s -> Exit),s -> Exit
        POP, // (Word,s -> Exit),s -> Exit
        PUSH1,
        1, // Word,(Word,s -> Exit),s -> Exit
        SWAP1, // (Word,s -> Exit),Word,s -> Exit
        JUMP // Word,s -> Exit
    }));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);

    ASSERT_EQ(blocks.size(), 5);

    ASSERT_TRUE(std::holds_alternative<JumpI>(blocks[0].terminator));
    ASSERT_TRUE(weak_equal(
        std::get<JumpI>(blocks[0].terminator).jump_kind,
        std::get<JumpI>(blocks[0].terminator).fallthrough_kind));
    ASSERT_TRUE(alpha_equal(
        std::get<JumpI>(blocks[0].terminator).jump_kind,
        cont_kind({word, cont(cont_kind({word}, 0))}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[0].kind, cont_kind({word, cont(cont_kind({word}, 0))}, 0)));

    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[1].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[1].terminator).jump_kind,
        cont_kind(
            {word,
             cont(cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)),
             word,
             cont(cont_kind({word}, 0))},
            0)));
    ASSERT_TRUE(alpha_equal(
        blocks[1].kind, cont_kind({word, cont(cont_kind({word}, 0))}, 0)));

    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[2].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[2].terminator).jump_kind,
        cont_kind(
            {word,
             cont(cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)),
             word,
             cont(cont_kind({word}, 0))},
            0)));
    ASSERT_TRUE(alpha_equal(
        blocks[2].kind,
        cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)));

    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[3].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[3].terminator).jump_kind, cont_kind({word}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[3].kind,
        cont_kind({word, word, cont(cont_kind({word}, 0))}, 0)));

    ASSERT_TRUE(std::holds_alternative<Jump>(blocks[4].terminator));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[4].terminator).jump_kind, cont_kind({word}, 0)));
    ASSERT_TRUE(alpha_equal(
        blocks[4].kind,
        cont_kind({kind_var(0), cont(cont_kind({word}, 0))}, 0)));
}

TEST(infer, crash_1)
{
    auto ir = local_stacks::LocalStacksIR(basic_blocks::BasicBlocksIR(
        {JUMPDEST, ADDRESS, JUMPDEST, PUSH0, ADDRESS, JUMP}));
    std::vector<Block> blocks = infer_types(ir.jumpdests, ir.blocks);
    ASSERT_EQ(blocks.size(), 2);
    ASSERT_TRUE(alpha_equal(blocks[1].kind, cont_words));
    ASSERT_TRUE(alpha_equal(
        std::get<Jump>(blocks[1].terminator).jump_kind, cont_words));
}
