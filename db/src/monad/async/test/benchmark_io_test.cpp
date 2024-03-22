#include <CLI/CLI.hpp>

#include <monad/async/config.hpp>
#include <monad/async/io.hpp>
#include <monad/async/io_senders.hpp>
#include <monad/async/storage_pool.hpp>
#include <monad/core/small_prng.hpp>
#include <monad/io/buffers.hpp>
#include <monad/io/ring.hpp>

#include <chrono>
#include <iostream>
#include <vector>

#include <sys/capability.h>

/* Throughput maximising with complete disregard to latency:


- For Intel Corporation Optane SSD 900P:

src/monad/async/test/benchmark_io_test --storage /dev/mapper/xpoint-rawblk2 \
--kernel-poll-thread 15 --enable-io-polling --ring-entries 32

Total ops/sec: 531845 mean latency: 107116 min: 29830 max: 585684

eager completions is a little slower.


- For Samsung Electronics Co Ltd NVMe SSD Controller PM9A1/PM9A3/980PRO:

src/monad/async/test/benchmark_io_test --storage /dev/mapper/raid0-rawblk0 \
--kernel-poll-thread 15 --ring-entries 256

Total ops/sec: 590681 mean latency: 568522 min: 84719 max: 2.08905e+06

i/o polling is slower, eager completions is a lot slower.


- For the peach27 device (Micron Technology Inc 7450 PRO NVMe SSD):

src/monad/async/test/benchmark_io_test --storage /dev/mapper/raid0-rawblk0 \
--kernel-poll-thread 15 --ring-entries 256 --eager-completions

Total ops/sec: 856148 mean latency: 322514 min: 7304 max: 971711

i/o polling appears to make no difference for this machine.

*/

/***************************************************************************/

/* Throughput maximising without significantly increasing mean and max latency:


- For Intel Corporation Optane SSD 900P:

src/monad/async/test/benchmark_io_test --storage /dev/mapper/xpoint-rawblk2 \
--workload 0 --concurrent-io 16 --eager-completions --enable-io-polling

Total ops/sec: 372308 mean latency: 12061.7 min: 8230 max: 79229

This is 70% of the maximum possible throughput above. Highest i/o priority has
no effect.


- For Samsung Electronics Co Ltd NVMe SSD Controller PM9A1/PM9A3/980PRO:

src/monad/async/test/benchmark_io_test --storage /dev/mapper/raid0-rawblk0 \
--workload 0 --concurrent-io 16 --eager-completions --enable-io-polling

Total ops/sec: 366382 mean latency: 13792.9 min: 11070 max: 129159

This is 62% of the maximum possible throughput above. Highest i/o priority cuts
max latency by about 20%.


- For the peach27 device (Micron Technology Inc 7450 PRO NVMe SSD):

src/monad/async/test/benchmark_io_test --storage /dev/mapper/raid0-rawblk0 \
--workload 0 --concurrent-io 64 --eager-completions --enable-io-polling

Total ops/sec: 605389 mean latency: 12385.3 min: 130 max: 203562

This is 70% of the maximum possible throughput above. Highest i/o priority has
no effect.
*/

/***************************************************************************/

/* fio verification of above results, same config as immediately above:

- For Intel Corporation Optane SSD 900P:

fio no SQPOLL:
Total ops/sec: 367000 mean latency: 43390 min: 12000 max: 184000

benchmark_io_test no SQPOLL:
Total ops/sec: 343724 mean latency: 13338.5 min: 9289 max: 107679

fio with SQPOLL:
Total ops/sec: 494000 mean latency: 32210 min: 13000 max: 222000

benchmark_io_test with SQPOLL:
Total ops/sec: 507668 mean latency: 30479.3 min: 11860 max: 113338



- For Samsung Electronics Co Ltd NVMe SSD Controller PM9A1/PM9A3/980PRO:

fio no SQPOLL:
Total ops/sec: 358000 mean latency: 44430 min: 13000 max: 7877000

benchmark_io_test no SQPOLL:
Total ops/sec: 334397 mean latency: 14738.1 min: 11410 max: 136048

fio with SQPOLL:
Total ops/sec: 518000 mean latency: 30680 min: 12000 max: 7908000

benchmark_io_test with SQPOLL:
Total ops/sec: 530874 mean latency: 29019.5 min: 12080 max: 160468

*/

struct shared_state_t
{
    size_t const chunk_count;
    uint32_t const chunk_capacity_div_disk_page_size;

    bool done{false};
    uint32_t ops{0};
    uint64_t min_ns, max_ns, acc_ns;
    monad::small_prng rand;

    constexpr shared_state_t(size_t const chunk_count_, size_t chunk_capacity_)
        : chunk_count(chunk_count_)
        , chunk_capacity_div_disk_page_size(
              uint32_t(chunk_capacity_ / MONAD_ASYNC_NAMESPACE::DISK_PAGE_SIZE))
    {
    }
};

struct receiver_t
{
    static constexpr bool lifetime_managed_internally = false;

    shared_state_t *shared;

    explicit receiver_t(shared_state_t *shared_)
        : shared(shared_)
    {
    }

    inline void set_value(
        MONAD_ASYNC_NAMESPACE::erased_connected_operation *rawstate,
        MONAD_ASYNC_NAMESPACE::read_single_buffer_sender::result_type buffer);

    void reset() {}
};

using connected_state_ptr_type =
    MONAD_ASYNC_NAMESPACE::AsyncIO::connected_operation_unique_ptr_type<
        MONAD_ASYNC_NAMESPACE::read_single_buffer_sender, receiver_t>;

inline void receiver_t::set_value(
    MONAD_ASYNC_NAMESPACE::erased_connected_operation *rawstate,
    MONAD_ASYNC_NAMESPACE::read_single_buffer_sender::result_type buffer)
{
    if (!buffer) {
        std::cerr << "FATAL: " << buffer.assume_error().message().c_str()
                  << std::endl;
    }
    MONAD_ASSERT(buffer);
    shared->ops++;
    auto const elapsed_ns(uint64_t(
        std::chrono::duration_cast<std::chrono::nanoseconds>(rawstate->elapsed)
            .count()));
    if (elapsed_ns < shared->min_ns) {
        shared->min_ns = elapsed_ns;
    }
    if (elapsed_ns > shared->max_ns) {
        shared->max_ns = elapsed_ns;
    }
    shared->acc_ns += elapsed_ns;
    if (!shared->done) {
        auto *state =
            static_cast<connected_state_ptr_type::element_type *>(rawstate);
        auto r = shared->rand();
        auto chunk_id = r % shared->chunk_count;
        auto offset_into_chunk =
            (r >> 16) % shared->chunk_capacity_div_disk_page_size;
        MONAD_ASYNC_NAMESPACE::chunk_offset_t offset(
            uint32_t(chunk_id),
            offset_into_chunk * MONAD_ASYNC_NAMESPACE::DISK_PAGE_SIZE);
        auto const io_priority = state->io_priority();
        state->reset(
            std::tuple{offset, MONAD_ASYNC_NAMESPACE::DISK_PAGE_SIZE},
            std::tuple{});
        state->set_io_priority(io_priority);
        state->initiate();
    }
}

int main(int argc, char *argv[])
{
    CLI::App cli("Tool for benchmarking the i/o engine", "benchmark_io_test");
    cli.footer(R"(Suitable sources of block storage:

1. Raw partitions on a storage device.
2. The storage device itself.
3. A file on a filing system (use 'truncate -s 1T sparsefile' to create and
set it to the desired size beforehand).
)");
    try {
        bool destroy_and_fill = false;
        std::vector<std::filesystem::path> storage_paths;
        monad::io::RingConfig ringconfig{128};
        unsigned concurrent_io = 2048;
        unsigned concurrent_read_io_limit = 0;
        bool eager_completions = false;
        bool highest_io_priority = false;
        unsigned workload_us = 5;
        unsigned duration_secs = 30;
        cli.add_option(
               "--storage",
               storage_paths,
               "one or more sources of block storage (must be at least 256Mb + "
               "4Kb long).")
            ->required();
        cli.add_flag(
            "--fill",
            destroy_and_fill,
            "destroy all existing contents, fill all chunks to full before "
            "doing test.");
        cli.add_option(
            "--concurrent-io",
            concurrent_io,
            "how many i/o this test program should do concurrently. Default is "
            "2048.");

        cli.add_option(
            "--ring-entries",
            ringconfig.entries,
            "how many submission entries io_uring should have. Default is "
            "128.");
        cli.add_flag(
            "--enable-io-polling",
            ringconfig.enable_io_polling,
            "whether to enable i/o polling within the kernel. Default is no "
            "i/o polling.");
        cli.add_option(
            "--kernel-poll-thread",
            ringconfig.sq_thread_cpu,
            "on what CPU to run a spin polling thread within the kernel. "
            "Default is no kernel thread.");
        cli.add_option(
            "--concurrent-read-io-limit",
            concurrent_read_io_limit,
            "maximum number of read i/o to issue at a time. This differs from "
            "--concurrent-io because it tells AsyncIO to ensure concurrent i/o "
            "does not exceed this, whereas --concurrent-io tells this test "
            "program how many i/o to do. Default is no "
            "limit.");
        cli.add_flag(
            "--eager-completions",
            eager_completions,
            "whether to reap completions as eagerly as possible. Default is "
            "reaping as needed.");
        cli.add_flag(
            "--highest-io-priority",
            highest_io_priority,
            "whether to set highest i/o priority possible, which is sent on to "
            "the storage device and may encourage it to minimise latency. "
            "Default is not set.");
        cli.add_option(
            "--workload",
            workload_us,
            "how long the simulated workload should last each time in "
            "microseconds. Default is five microseconds.");
        cli.add_option(
            "--duration",
            duration_secs,
            "how long the benchmark should run for in seconds. Default is "
            "thirty seconds.");
        cli.parse(argc, argv);

        if (highest_io_priority) {
            // We will need the CAP_SYS_NICE capability for this to work
            MONAD_ASSERT(CAP_IS_SUPPORTED(CAP_SYS_NICE));
            auto caps = cap_get_proc();
            MONAD_ASSERT(caps != nullptr);
            auto uncaps =
                monad::make_scope_exit([&]() noexcept { cap_free(caps); });
            cap_value_t const cap_list[] = {CAP_SYS_NICE};
            if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_SET) == -1 ||
                cap_set_proc(caps) == -1) {
                std::cerr
                    << "FATAL: To use --highest-io-priority the process needs "
                       "the CAP_SYS_NICE capability. To assign that, "
                       "do:\n\nsudo setcap cap_sys_nice+ep "
                       "benchmark_io_test\n\nAnd run it again."
                    << std::endl;
                return 1;
            }
        }

        auto const mode =
            destroy_and_fill
                ? MONAD_ASYNC_NAMESPACE::storage_pool::mode::truncate
                : MONAD_ASYNC_NAMESPACE::storage_pool::mode::open_existing;
        MONAD_ASYNC_NAMESPACE::storage_pool::creation_flags flags;
        flags.interleave_chunks_evenly = true;
        MONAD_ASYNC_NAMESPACE::storage_pool pool{{storage_paths}, mode, flags};

        monad::io::Ring ring(ringconfig);
        monad::io::Buffers rwbuf = monad::io::make_buffers_for_read_only(
            ring,
            concurrent_io,
            MONAD_ASYNC_NAMESPACE::AsyncIO::READ_BUFFER_SIZE);
        auto io = MONAD_ASYNC_NAMESPACE::AsyncIO{pool, rwbuf};
        if (destroy_and_fill) {
            for (uint32_t n = 0; n < io.chunk_count(); n++) {
                pool.chunk(pool.seq, n)
                    ->write_fd(uint32_t(io.chunk_capacity(n)));
            }
        }
        io.set_capture_io_latencies(true);
        io.set_concurrent_read_io_limit(concurrent_read_io_limit);

        shared_state_t shared_state{
            io.chunk_count(), uint32_t(io.chunk_capacity(0))};
        std::vector<connected_state_ptr_type> states;
        states.reserve(concurrent_io);
        for (size_t n = 0; n < concurrent_io; n++) {
            states.push_back(io.make_connected(
                MONAD_ASYNC_NAMESPACE::read_single_buffer_sender({0, 0}, 0),
                receiver_t{&shared_state}));
        }

        struct statistics_t
        {
            uint32_t ops_per_sec{0};
            float mean_latency{0};
            float min_latency{0};
            float max_latency{0};
        } statistics;

        auto const begin = std::chrono::steady_clock::now();
        auto print_statistics = [&] {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed =
                double(std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - begin)
                           .count());
            statistics.ops_per_sec =
                uint32_t(1000.0 * double(shared_state.ops) / elapsed);
            statistics.mean_latency =
                float(double(shared_state.acc_ns) / double(shared_state.ops));
            statistics.min_latency = float(shared_state.min_ns);
            statistics.max_latency = float(shared_state.max_ns);
            std::cout << "\nTotal ops/sec: " << statistics.ops_per_sec
                      << " mean latency: " << statistics.mean_latency
                      << " min: " << statistics.min_latency
                      << " max: " << statistics.max_latency << std::endl;
        };

        for (auto &i : states) {
            MONAD_ASYNC_NAMESPACE::filled_read_buffer res;
            if (highest_io_priority) {
                i->set_io_priority(
                    MONAD_ASYNC_NAMESPACE::erased_connected_operation::
                        io_priority::highest);
            }
            i->receiver().set_value(
                i.get(),
                MONAD_ASYNC_NAMESPACE::read_single_buffer_sender::result_type{
                    res});
        }
        io.set_eager_completions(eager_completions);
        shared_state.acc_ns = 0;
        shared_state.max_ns = 0;
        shared_state.min_ns = UINT64_MAX;
        std::chrono::seconds elapsed_secs(2);
        do {
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - begin);
            if (diff > elapsed_secs) {
                print_statistics();
                elapsed_secs = diff;
            }
            io.poll_nonblocking(1);
            if (workload_us > 0) {
                auto const begin2 = std::chrono::steady_clock::now();
                do {
                    /* deliberately occupy the CPU fully */
                }
                while (std::chrono::steady_clock::now() - begin2 <
                       std::chrono::microseconds(workload_us));
            }
        }
        while (std::chrono::steady_clock::now() - begin <
               std::chrono::seconds(duration_secs));
        shared_state.done = true;
        io.wait_until_done();
        print_statistics();
    }

    catch (const CLI::CallForHelp &e) {
        std::cout << cli.help() << std::flush;
    }

    catch (const CLI::RequiredError &e) {
        std::cerr << "FATAL: " << e.what() << "\n\n";
        std::cerr << cli.help() << std::flush;
        return 1;
    }

    catch (std::exception const &e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
