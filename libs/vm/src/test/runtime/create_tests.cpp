#include "fixture.h"
#include "intx/intx.hpp"

#include <runtime/create.h>
#include <runtime/memory.h>
#include <runtime/transmute.h>
#include <utils/uint256.h>

#include <evmc/evmc.h>

using namespace monad;
using namespace monad::runtime;
using namespace monad::compiler::test;

using namespace intx;

constexpr utils::uint256_t prog = 0x63FFFFFFFF6000526004601CF3_u256;
constexpr evmc_address result_addr = {0x42};

TEST_F(RuntimeTest, CreateFrontier)
{
    constexpr auto rev = EVMC_FRONTIER;
    call(mstore<rev>, 0, prog);
    ASSERT_EQ(ctx_.memory.data[31], 0xF3);

    ctx_.gas_remaining = 1000000;
    host_.call_result = create_result(result_addr, 900000, 10);

    auto do_create = wrap(create<rev>);

    utils::uint256_t const addr = do_create(0, 19, 13);

    ASSERT_EQ(addr, uint256_from_address(result_addr));

    ASSERT_EQ(ctx_.gas_remaining, 900000);
    ASSERT_EQ(ctx_.gas_refund, 10);
}

TEST_F(RuntimeTest, CreateShanghai)
{
    constexpr auto rev = EVMC_SHANGHAI;
    call(mstore<rev>, 0, prog);
    ASSERT_EQ(ctx_.memory.data[31], 0xF3);

    ctx_.gas_remaining = 1000000;
    host_.call_result = create_result(result_addr, 900000, 10);

    auto do_create = wrap(create<rev>);

    utils::uint256_t const addr = do_create(0, 19, 13);

    ASSERT_EQ(addr, uint256_from_address(result_addr));

    ASSERT_EQ(ctx_.gas_remaining, 915624);
    ASSERT_EQ(ctx_.gas_refund, 10);
}

TEST_F(RuntimeTest, CreateTangerineWhistle)
{
    constexpr auto rev = EVMC_TANGERINE_WHISTLE;
    call(mstore<rev>, 0, prog);
    ASSERT_EQ(ctx_.memory.data[31], 0xF3);

    ctx_.gas_remaining = 1000000;
    host_.call_result = create_result(result_addr, 900000, 10);

    auto do_create = wrap(create<rev>);

    utils::uint256_t const addr = do_create(0, 19, 13);

    ASSERT_EQ(addr, uint256_from_address(result_addr));

    ASSERT_EQ(ctx_.gas_remaining, 915625);
    ASSERT_EQ(ctx_.gas_refund, 10);
}

TEST_F(RuntimeTest, CreateFrontierSizeIsZero)
{
    constexpr auto rev = EVMC_FRONTIER;

    ctx_.gas_remaining = 1000000;
    host_.call_result = create_result(result_addr, 900000);

    auto do_create = wrap(create<rev>);

    utils::uint256_t const addr = do_create(0, 0, 0);

    ASSERT_EQ(addr, uint256_from_address(result_addr));
    ASSERT_EQ(ctx_.gas_remaining, 900000);
}

TEST_F(RuntimeTest, CreateFrontierFailure)
{
    constexpr auto rev = EVMC_FRONTIER;

    host_.call_result = failure_result(EVMC_OUT_OF_GAS);

    auto do_create = wrap(create<rev>);

    utils::uint256_t const addr = do_create(0, 0, 0);

    ASSERT_EQ(addr, 0);
}

TEST_F(RuntimeTest, Create2Constantinople)
{
    constexpr auto rev = EVMC_CONSTANTINOPLE;
    call(mstore<rev>, 0, prog);
    ASSERT_EQ(ctx_.memory.data[31], 0xF3);

    ctx_.gas_remaining = 1000000;
    host_.call_result = create_result(result_addr, 900000, 10);

    auto do_create2 = wrap(create2<rev>);

    utils::uint256_t const addr = do_create2(0, 19, 13, 0x99);

    ASSERT_EQ(addr, uint256_from_address(result_addr));

    ASSERT_EQ(ctx_.gas_remaining, 915624);
    ASSERT_EQ(ctx_.gas_refund, 10);
}
