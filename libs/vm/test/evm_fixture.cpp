#include "evm_fixture.hpp"

#include <monad/compiler/types.hpp>
#include <monad/utils/assert.h>
#include <monad/vm/evmone/baseline_execute.hpp>
#include <monad/vm/evmone/code_analysis.hpp>

#include <evmc/bytes.hpp>
#include <evmc/evmc.h>
#include <evmc/evmc.hpp>

#include <intx/intx.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <utility>

namespace monad::compiler::test
{
    void EvmTest::pre_execute(
        std::int64_t gas_limit, std::span<std::uint8_t const> calldata) noexcept
    {
        result_ = evmc::Result();
        output_data_ = {};

        host_.accounts[msg_.sender].balance = intx::be::store<evmc::bytes32>(
            std::numeric_limits<uint256_t>::max());

        msg_.gas = gas_limit;
        msg_.input_data = calldata.data();
        msg_.input_size = calldata.size();

        if (rev_ >= EVMC_BERLIN) {
            host_.access_account(msg_.sender);
            host_.access_account(msg_.recipient);
        }
    }

    void EvmTest::execute(
        std::int64_t gas_limit, std::span<std::uint8_t const> code,
        std::span<std::uint8_t const> calldata, Implementation impl) noexcept
    {
        pre_execute(gas_limit, calldata);

        if (impl == Compiler) {
            result_ = evmc::Result(vm_.compile_and_execute(
                &host_.get_interface(),
                host_.to_context(),
                rev_,
                &msg_,
                code.data(),
                code.size()));
        }
        else {
            MONAD_COMPILER_ASSERT(impl == Evmone);

            result_ = monad::baseline_execute(
                msg_, rev_, &host_, monad::analyze(evmc::bytes_view(code)));
        }
    }

    void EvmTest::execute(
        std::int64_t gas_limit, std::initializer_list<std::uint8_t> code,
        std::span<std::uint8_t const> calldata, Implementation impl) noexcept
    {
        execute(gas_limit, std::span{code}, calldata, impl);
    }

    void EvmTest::execute(
        std::span<std::uint8_t const> code,
        std::span<std::uint8_t const> calldata, Implementation impl) noexcept
    {
        execute(std::numeric_limits<std::int64_t>::max(), code, calldata, impl);
    }

    void EvmTest::execute(
        std::initializer_list<std::uint8_t> code,
        std::span<std::uint8_t const> calldata, Implementation impl) noexcept
    {
        execute(std::span{code}, calldata, impl);
    }

    void EvmTest::execute_and_compare(
        std::int64_t gas_limit, std::span<std::uint8_t const> code,
        std::span<std::uint8_t const> calldata) noexcept
    {
        // This comparison shouldn't be called multiple times in one test; if
        // any state has been recorded on this host before we begin a test, the
        // test should fail and stop us from trying to make assertions about a
        // broken state.
        ASSERT_TRUE(has_empty_state());

        execute(gas_limit, code, calldata, Compiler);
        auto actual = std::move(result_);

        // We need to reset the host between executions; otherwise the state
        // maintained will produce inconsistent results (e.g. an account is
        // touched by the first run, then is subsequently warm for the second
        // one).
        host_ = {};

        execute(gas_limit, code, calldata, Evmone);
        auto expected = std::move(result_);

        switch (expected.status_code) {
        case EVMC_SUCCESS:
        case EVMC_REVERT:
            ASSERT_EQ(actual.status_code, expected.status_code);
            break;
        default:
            ASSERT_NE(actual.status_code, EVMC_SUCCESS);
            ASSERT_NE(actual.status_code, EVMC_REVERT);
            break;
        }

        ASSERT_EQ(actual.gas_left, expected.gas_left);
        ASSERT_EQ(actual.gas_refund, expected.gas_refund);
        ASSERT_EQ(actual.output_size, expected.output_size);

        ASSERT_TRUE(std::equal(
            actual.output_data,
            actual.output_data + actual.output_size,
            expected.output_data));

        ASSERT_EQ(
            evmc::address(actual.create_address),
            evmc::address(expected.create_address));
    }

    void EvmTest::execute_and_compare(
        std::int64_t gas_limit, std::initializer_list<std::uint8_t> code,
        std::span<std::uint8_t const> calldata) noexcept
    {
        execute_and_compare(gas_limit, std::span{code}, calldata);
    }

    bool EvmTest::has_empty_state() const noexcept
    {
        return host_.accounts.empty() &&
               host_.recorded_account_accesses.empty() &&
               host_.recorded_blockhashes.empty() &&
               host_.recorded_calls.empty() && host_.recorded_logs.empty() &&
               host_.recorded_selfdestructs.empty();
    }
}
