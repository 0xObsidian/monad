#include "test_fixtures_base.hpp"
#include "test_fixtures_gtest.hpp"

#include <monad/async/config.hpp>
#include <monad/mpt/config.hpp>
#include <monad/mpt/node.hpp>
#include <monad/mpt/trie.hpp>
#include <monad/mpt/update.hpp>

#include <monad/test/gtest_signal_stacktrace_printer.hpp> // NOLINT

#include <cstddef>
#include <iostream>
#include <ostream>
#include <utility>
#include <vector>

using namespace MONAD_ASYNC_NAMESPACE;
using namespace MONAD_MPT_NAMESPACE;
using namespace monad::literals;

static constexpr size_t CHUNKS_TO_FILL = 8;

struct CompactionTest
    : public monad::test::FillDBWithChunksGTest<
          monad::test::FillDBWithChunksConfig{.chunks_to_fill = CHUNKS_TO_FILL}>
{
};

TEST_F(CompactionTest, first_chunk_is_compacted)
{
    std::vector<Update> updates;
    auto const fast_list_ids = state()->fast_list_ids();
    for (auto &i : state()->keys) {
        if (i.second > fast_list_ids[0].first) {
            break;
        }
        updates.push_back(make_update(i.first, UpdateList{}));
    }
    std::cout << "Erasing the first " << updates.size()
              << " inserted keys, which should enable the whole of the "
                 "first block to be compacted away."
              << std::endl;
    UpdateList update_ls;
    for (auto &i : updates) {
        update_ls.push_front(i);
    }
    state()->root = upsert(
        state()->aux,
        state()->sm,
        std::move(state()->root),
        std::move(update_ls));
    std::cout << "\nBefore compaction:";
    state()->print(std::cout);
    // TODO DO COMPACTION
    // TODO CHECK POOL'S FIRST CHUNK WAS DEFINITELY RELEASED
}
