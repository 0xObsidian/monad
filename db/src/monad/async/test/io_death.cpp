#include "gtest/gtest.h"

#include <monad/async/concepts.hpp>
#include <monad/async/config.hpp>
#include <monad/async/connected_operation.hpp>
#include <monad/async/erased_connected_operation.hpp>
#include <monad/async/io.hpp>
#include <monad/async/io_senders.hpp>
#include <monad/async/storage_pool.hpp>
#include <monad/core/assert.h>
#include <monad/io/buffers.hpp>
#include <monad/io/ring.hpp>

// DO NOT include gtest_signal_stacktrace_printer.hpp here, it interferes with
// Google Test's death test handling

#include <csignal>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace
{
    TEST(AsyncIODeathTest, write_buffer_exhaustion_causes_death)
    {
        // It complains about there being two threads without this, despite the
        // fact that very clearly there is exactly one thread and gdb agrees.
        testing::FLAGS_gtest_death_test_style = "threadsafe";

        monad::async::storage_pool pool(
            monad::async::use_anonymous_inode_tag{});
        monad::io::Ring testring(128, 0);
        monad::io::Buffers testrwbuf{
            testring,
            1,
            1,
            monad::async::AsyncIO::MONAD_IO_BUFFERS_READ_SIZE,
            monad::async::AsyncIO::MONAD_IO_BUFFERS_WRITE_SIZE};
        monad::async::AsyncIO testio(pool, testring, testrwbuf);

        struct empty_receiver
        {
            enum
            {
                lifetime_managed_internally = true
            };

            void set_value(
                monad::async::erased_connected_operation *,
                monad::async::write_single_buffer_sender::result_type r)
            {
                MONAD_ASSERT(r);
            }
        };

        for (size_t n = 0; n < 10; n++) {
            auto make = [&] {
                auto state(testio.make_connected(
                    monad::async::write_single_buffer_sender(
                        {0, 0}, monad::async::DISK_PAGE_SIZE),
                    empty_receiver{}));
                // Exactly the same test as the non-death test, except for this
                // line
                // state->initiate();
                state.release();
            };
            if (n > 0) {
                EXPECT_EXIT(make(), ::testing::KilledBySignal(SIGABRT), ".*");
            }
            else {
                make();
            }
        }
    }

    TEST(AsyncIODeathTest, read_buffer_exhaustion_causes_death)
    {
        // It complains about there being two threads without this, despite the
        // fact that very clearly there is exactly one thread and gdb agrees.
        testing::FLAGS_gtest_death_test_style = "threadsafe";

        monad::async::storage_pool pool(
            monad::async::use_anonymous_inode_tag{});
        monad::io::Ring testring(128, 0);
        monad::io::Buffers testrwbuf{
            testring,
            1,
            1,
            monad::async::AsyncIO::MONAD_IO_BUFFERS_READ_SIZE,
            monad::async::AsyncIO::MONAD_IO_BUFFERS_WRITE_SIZE};
        monad::async::AsyncIO testio(pool, testring, testrwbuf);
        std::vector<monad::async::read_single_buffer_sender::buffer_type> bufs;

        struct empty_receiver
        {
            std::vector<monad::async::read_single_buffer_sender::buffer_type>
                &bufs;

            enum
            {
                lifetime_managed_internally = true
            };

            void set_value(
                monad::async::erased_connected_operation *,
                monad::async::read_single_buffer_sender::result_type r)
            {
                MONAD_ASSERT(r);
                // Exactly the same test as the death test, except for this line
                bufs.emplace_back(std::move(r.assume_value().get()));
            }
        };

        auto make = [&] {
            auto state(testio.make_connected(
                monad::async::read_single_buffer_sender(
                    {0, 0}, monad::async::DISK_PAGE_SIZE),
                empty_receiver{bufs}));
            state->initiate(); // will reap completions if no buffers free
            state.release();
        };
        for (size_t n = 0; n < 512; n++) {
            make();
        }
        EXPECT_EXIT(make(), ::testing::KilledBySignal(SIGABRT), ".*");
    }
}
