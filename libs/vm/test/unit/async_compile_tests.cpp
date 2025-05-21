#include <monad/vm/code.hpp>
#include <monad/vm/compiler.hpp>
#include <monad/vm/compiler/types.hpp>
#include <monad/vm/evm/opcodes.hpp>
#include <monad/vm/runtime/types.hpp>

#include <evmc/evmc.h>
#include <evmc/evmc.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace monad::vm;
using namespace monad::vm::compiler;

namespace
{
    std::vector<uint8_t> test_code(uint64_t index)
    {
        std::vector<uint8_t> code = {PUSH1, 1, PUSH8};
        for (uint64_t i = 0; i < 8; ++i) {
            code.push_back(static_cast<uint8_t>(index >> 8 * (7 - i)));
        }
        code.push_back(RETURN);
        return code;
    }

    evmc::bytes32 test_hash(uint64_t index)
    {
        evmc::bytes32 h{};
        for (uint64_t i = 0; i < 8; ++i) {
            h.bytes[31 - i] = static_cast<uint8_t>(index >> 8 * (7 - i));
        }
        return h;
    }
}

TEST(async_compile_test, stress)
{
    constexpr evmc_revision R = EVMC_CANCUN;
    constexpr size_t P = 10;
    constexpr size_t L = 120;
    constexpr size_t N = L * 12;

    Compiler compiler{L};

    auto first_start_time = std::chrono::steady_clock::now();
    compiler.compile(R, make_shared_intercode(test_code(2 * N)));
    auto first_end_time = std::chrono::steady_clock::now();
    auto compile_time_estimate = first_end_time - first_start_time;

    auto producer = [&](uint64_t start_index) {
        std::unordered_set<uint64_t> producer_set;
        // Spam async compiler with `L` async compilation requests followed
        // by a sleep period to let the compiler partially empty the queue.
        for (uint64_t i = 0; i < N;) {
            uint64_t const c = std::min(i + L, N);
            for (; i < c; ++i) {
                uint64_t const index = start_index + i;
                auto code = test_code(index);
                auto hash = test_hash(index);
                auto icode = make_shared_intercode(std::move(code));
                if (compiler.async_compile(R, hash, icode)) {
                    auto [_, inserted] = producer_set.insert(index);
                    ASSERT_TRUE(inserted);
                }
            }
            std::this_thread::sleep_for(compile_time_estimate * L / 4);
        }

        compiler.debug_wait_for_empty_queue();

        for (uint64_t const index : producer_set) {
            auto vcode = compiler.find_varcode(test_hash(index));
            ASSERT_TRUE(vcode.has_value());
            auto ncode = (*vcode)->nativecode();
            ASSERT_TRUE(!!ncode);

            auto entry = ncode->entrypoint();
            ASSERT_TRUE(entry != nullptr);

            auto ctx = runtime::Context::empty();
            ctx.gas_remaining = 100;
            entry(&ctx, nullptr);

            auto const &ret = ctx.result;
            ASSERT_EQ(ret.status, runtime::StatusCode::Success);
            ASSERT_EQ(uint256_t::load_le(ret.offset), index);
            ASSERT_EQ(uint256_t::load_le(ret.size), 1);
        }
    };

    std::vector<std::thread> producers;
    for (size_t i = 0; i < P; ++i) {
        producers.emplace_back(producer, (i * N) / 2);
    }
    for (size_t i = 0; i < P; ++i) {
        producers[i].join();
    }
}
