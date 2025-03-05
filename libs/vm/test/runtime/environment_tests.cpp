#include "fixture.hpp"

#include <monad/runtime/environment.hpp>

#include <evmc/evmc.hpp>

#include <intx/intx.hpp>

using namespace monad::runtime;
using namespace monad::compiler::test;
using namespace intx;

TEST_F(RuntimeTest, SelfBalance)
{
    host_.accounts[0x0000000000000000000000000000000000000001_address]
        .set_balance(100);

    ASSERT_EQ(call(selfbalance), 100);
}

TEST_F(RuntimeTest, BlockHashOld)
{
    ASSERT_EQ(call(blockhash, 1000), 0);
    ASSERT_EQ(call(blockhash, 23527), 0);
}

TEST_F(RuntimeTest, BlockHashCurrent)
{
    constexpr auto hash =
        0x105DF6064F84551C4100A368056B8AF0E491077245DAB1536D2CFA6AB78421CE_u256;

    ASSERT_EQ(call(blockhash, 23528), hash);
    ASSERT_EQ(call(blockhash, 23660), hash);
    ASSERT_EQ(call(blockhash, 23783), hash);
}

TEST_F(RuntimeTest, BlockHashNew)
{
    ASSERT_EQ(call(blockhash, 23784), 0);
    ASSERT_EQ(call(blockhash, 30000), 0);
}

TEST_F(RuntimeTest, BlobHash)
{
    ASSERT_EQ(call(blobhash, 0), 1);
    ASSERT_EQ(call(blobhash, 1), 2);
    ASSERT_EQ(call(blobhash, 2), 0);
    ASSERT_EQ(call(blobhash, 3), 0);
}
