#include <monad/vm/core/assert.h>
#include <monad/vm/utils/lru_weight_cache.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace monad::vm::utils;

namespace
{
    using Key = uint32_t;

    struct Value
    {
        uint32_t value;

        uint32_t cache_weight() const
        {
            return value;
        }
    };

    using WeightCache = LruWeightCache<Key, Value>;

    constexpr uint32_t max_weight{20'000};
    constexpr auto update_period = std::chrono::nanoseconds{10'000};
    constexpr Key base_key{0};
    constexpr Value base_value{1};

    std::vector<Key> make_elems()
    {
        std::vector<Key> ks;
        for (Key k = 1; k < max_weight; ++k) {
            ks.push_back(k);
        }
        return ks;
    }

    std::function<Value(Key)> make_default_values()
    {
        std::vector<Value> vs;
        for (Key k = 1; k < max_weight; ++k) {
            vs.emplace_back(1 + (k & 1));
        }
        return [vs = std::move(vs)](Key k) { return vs[k - 1]; };
    }

    std::vector<Key> const elems = make_elems();
    std::function<Value(Key)> const default_values = make_default_values();

    struct TestThread : public std::thread
    {
        using std::thread::thread;

        TestThread(TestThread &&) = default;
        TestThread &operator=(TestThread &&) = default;

        ~TestThread()
        {
            if (joinable()) {
                join();
            }
        }
    };

    struct LruWeightCacheTest : public testing::Test
    {
        WeightCache weight_cache_{max_weight, update_period};
        std::atomic<uint64_t> current_weight_{0};

        std::optional<uint32_t> weight_cache_find(Key k)
        {
            WeightCache::ConstAccessor acc;
            if (!weight_cache_.find(acc, k)) {
                return std::nullopt;
            }
            return acc->second.value_.value;
        }

        TestThread make_tester()
        {
            return TestThread{[this] {
                for (;;) {
                    auto w = current_weight_.load();
                    if (w > max_weight - 3 * max_weight / 10) {
                        break;
                    }
                    auto v = weight_cache_find(base_key);
                    ASSERT_TRUE(v.has_value());
                    ASSERT_EQ(*v, base_value.value);
                }
            }};
        }

        std::vector<TestThread>
        make_readers(size_t reader_count, std::function<void()> p)
        {
            auto r = [this, p](size_t start_index) {
                size_t i = start_index;
                while (current_weight_.load() <= max_weight) {
                    Key const k = elems[i++ % elems.size()];
                    (void)weight_cache_find(k);
                }
                p();
            };
            std::vector<TestThread> readers;
            for (size_t i = 0; i < reader_count; ++i) {
                size_t start_index = i & 1 ? elems.size() / 5 : 0;
                readers.emplace_back(r, start_index);
            }
            return readers;
        }

        std::vector<TestThread> make_rereaders(
            std::unordered_map<Key, std::atomic_flag> &is_updated,
            size_t rereader_count)
        {
            for (auto &[_, b] : is_updated) {
                MONAD_VM_ASSERT(!b.test());
            }
            MONAD_VM_ASSERT(is_updated.size() == elems.size());
            auto r = [this, &is_updated](size_t start_index) {
                size_t i = start_index;
                for (;;) {
                    Key const k = elems[i++ % elems.size()];
                    if (current_weight_.load() + 1 >= max_weight) {
                        break;
                    }
                    auto v = weight_cache_find(k);
                    if (v.has_value() && !is_updated.at(k).test_and_set()) {
                        current_weight_.fetch_add(*v);
                    }
                }
            };
            std::vector<TestThread> rereaders;
            for (size_t i = 0; i < rereader_count; ++i) {
                size_t start_index = i & 1 ? elems.size() / 5 : 0;
                rereaders.emplace_back(r, start_index);
            }
            return rereaders;
        }

        std::vector<TestThread> make_writers(
            size_t writer_count, std::function<void()> p,
            std::function<Value(Key)> f)
        {
            auto w = [this, p, f](size_t start_index) {
                size_t i = start_index;
                for (;;) {
                    auto k = elems[i++ % elems.size()];
                    if (current_weight_.load() > max_weight) {
                        break;
                    }
                    auto v = f(k);
                    if (weight_cache_.insert(k, v)) {
                        current_weight_.fetch_add(v.cache_weight());
                    }
                }
                p();
            };
            std::vector<TestThread> writers;
            for (size_t i = 0; i < writer_count; ++i) {
                size_t start_index = i & 1 ? elems.size() / 5 : 0;
                writers.emplace_back(w, start_index);
            }
            return writers;
        }

        void insert_initial_base()
        {
            ASSERT_FALSE(weight_cache_find(base_key).has_value());

            weight_cache_.insert(base_key, base_value);

            auto original = weight_cache_find(base_key);
            ASSERT_TRUE(original.has_value());
            ASSERT_EQ(*original, base_value.value);

            ASSERT_EQ(current_weight_.load(), 0);
            current_weight_ = base_value.cache_weight();
        }
    };
}

TEST_F(LruWeightCacheTest, insert_find)
{
    insert_initial_base();
    auto tester = make_tester();
    auto readers = make_readers(10, [] {});
    auto writers = make_writers(10, [] {}, default_values);
}

TEST_F(LruWeightCacheTest, evict_1_writer)
{
    insert_initial_base();
    auto p = [this] { ASSERT_FALSE(weight_cache_find(base_key).has_value()); };
    auto readers = make_readers(10, p);
    auto writers = make_writers(1, p, default_values);
}

TEST_F(LruWeightCacheTest, evict_10_writers)
{
    insert_initial_base();
    auto p = [this] { ASSERT_FALSE(weight_cache_find(base_key).has_value()); };
    auto readers = make_readers(10, p);
    auto writers = make_writers(10, p, default_values);
}

TEST_F(LruWeightCacheTest, reread_evict)
{
    uint32_t init_weight = 0;
    for (auto k : elems) {
        auto v = default_values(k);
        weight_cache_.insert(k, v);
        init_weight += v.cache_weight();
        if (init_weight >= max_weight) {
            break;
        }
    }
    ASSERT_EQ(init_weight, max_weight);
    ASSERT_EQ(
        default_values(elems[0]).cache_weight(), base_value.cache_weight() + 1);

    insert_initial_base();

    std::unordered_map<Key, std::atomic_flag> is_updated;
    for (auto k : elems) {
        is_updated.emplace(k, false);
    }

    std::this_thread::sleep_for(10 * update_period);
    {
        auto rereaders = make_rereaders(is_updated, 10);
    }

    ASSERT_TRUE(weight_cache_.insert(elems[0], default_values(elems[0])));
    ASSERT_FALSE(weight_cache_find(base_key).has_value());
}

TEST_F(LruWeightCacheTest, is_consistent)
{
    for (uint32_t i = 0; i < 20; ++i) {
        {
            auto readers = make_readers(10, [] {});
            auto writers = make_writers(
                10,
                [] {},
                [i](Key k) {
                    return Value{1 + ((k + i + 3) & ((1 << 16) - 1))};
                });
        }
        ASSERT_TRUE(weight_cache_.unsafe_check_consistent());
        current_weight_ = 0;
    }
}
