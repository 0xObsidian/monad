#include "test_fixtures_base.hpp"
#include "test_fixtures_gtest.hpp"

#include <monad/core/byte_string.hpp>

#include <monad/core/assert.h>
#include <monad/core/hex_literal.hpp>
#include <monad/mpt/config.hpp>
#include <monad/mpt/update.hpp>

#if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstringop-overflow="
#endif
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>
#include <boost/json/visit.hpp>
#if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic pop
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ::monad::test;
using namespace MONAD_MPT_NAMESPACE;

template <typename TFixture>
struct ManyNestedUpdates : public TFixture
{
};

using TrieTypes = ::testing::Types<InMemoryTrieGTest, OnDiskTrieGTest>;
TYPED_TEST_SUITE(ManyNestedUpdates, TrieTypes);

inline ::boost::json::value read_corpus(std::string_view suffix)
{
    auto path = std::filesystem::path(__FILE__);
    path = std::filesystem::path(path).remove_filename() /
           (path.filename()
                .replace_extension()
                .string()
                .append("_")
                .append(suffix)
                .append(".json"));
    std::cout << "  read_corpus(" << path << ")" << std::endl;
    std::ifstream s(path);
    MONAD_ASSERT(s);
    return ::boost::json::parse(s);
}

inline monad::byte_string const &to_byte_string(std::string_view s)
{
    static std::map<std::string, monad::byte_string> storage;
    std::string key(s);
    auto it = storage.find(key);
    if (it == storage.end()) {
        it = storage.emplace(std::move(key), monad::from_hex(s)).first;
    }
    return it->second;
}

inline void
reserve_storage(std::vector<Update> &storage, ::boost::json::object const &m)
{
    storage.reserve(storage.capacity() + m.size());
    for (auto const &i : m) {
        ::boost::json::visit(
            [&](auto const &item) {
                using type = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<type, ::boost::json::object>) {
                    reserve_storage(storage, item);
                }
            },
            i.value());
    }
}
inline UpdateList
prepare_upsert(std::vector<Update> &storage, ::boost::json::object const &m)
{
    UpdateList next;
    for (auto const &i : m) {
        ::boost::json::visit(
            [&](auto const &item) {
                using type = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<type, ::boost::json::string>) {
                    storage.push_back(make_update(
                        to_byte_string(i.key()), to_byte_string(item)));
                }
                else if constexpr (std::
                                       is_same_v<type, ::boost::json::object>) {
                    storage.push_back(make_update(
                        to_byte_string(i.key()),
                        to_byte_string(item.at("value").as_string()),
                        false,
                        prepare_upsert(
                            storage, item.at("subtrie").as_object())));
                }
                else {
                    MONAD_ASSERT(false);
                }
            },
            i.value());
        next.push_front(storage.back());
    }
    return next;
}

template <class State>
inline void do_upsert_corpus(State *self, ::boost::json::object const &updates)
{
    for (auto const &i : updates) {
        ::boost::json::visit(
            [&](auto const &item) {
                using type = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<type, ::boost::json::string>) {
                    self->root = upsert_updates(
                        self->aux,
                        self->sm,
                        std::move(self->root),
                        make_update(
                            to_byte_string(i.key()), to_byte_string(item)));
                }
                else if constexpr (std::
                                       is_same_v<type, ::boost::json::object>) {
                    std::vector<Update> storage;
                    reserve_storage(storage, item.at("subtrie").as_object());
                    std::cout << "   Inserting key-value with "
                              << storage.capacity() << " updates ..."
                              << std::endl;
                    self->root = upsert_updates(
                        self->aux,
                        self->sm,
                        std::move(self->root),
                        make_update(
                            to_byte_string(i.key()),
                            to_byte_string(item.at("value").as_string()),
                            false,
                            prepare_upsert(
                                storage, item.at("subtrie").as_object())));
                }
                else {
                    MONAD_ASSERT(false);
                }
            },
            i.value());
    }
}

template <class State>
inline void do_erase_corpus(State *self, ::boost::json::object const &updates)
{
    for (auto const &i : updates) {
        self->root = upsert_updates(
            self->aux,
            self->sm,
            std::move(self->root),
            make_erase(to_byte_string(i.key())));
    }
}

TYPED_TEST(ManyNestedUpdates, simple_fixed_test_not_from_json)
{
    auto const key1 =
        0xac4c09c28206e7e35594aa6b342f5d0a3a5e4842fab428f762e6e282e5c1657c_hex;
    auto const val1 = 0xb36711eb3906a7c8603d71d409e7a54d87bdc1f70442027a5b_hex;
    auto const key2 =
        0x212b86b49e656acf0641169a0b59f4e629439f25d9d4654fec8d4819fb40d6ba_hex;
    auto const val2 = 0x1c441ae6_hex;

    this->root = upsert_updates(
        this->aux,
        this->sm,
        nullptr,
        make_update(key1, val1),
        make_update(key2, val2));
    EXPECT_EQ(
        this->root_hash(),
        0x0d203b1bed203d355d6201a703774018a182975fc4fcae0dae19825cd40ccd17_hex);
}

TYPED_TEST(ManyNestedUpdates, test_corpus_simple_flat)
{
    auto const updates = read_corpus("simple_flat").as_object();
    do_upsert_corpus(this, updates.at("updates").as_object());
    EXPECT_EQ(
        this->root_hash(), to_byte_string(updates.at("root_hash").as_string()));
    do_erase_corpus(this, updates.at("updates").as_object());
    EXPECT_EQ(
        this->root_hash(),
        0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421_hex /* null hash */);
}

TYPED_TEST(ManyNestedUpdates, test_corpus_0)
{
    auto const updates = read_corpus("src0").as_object();
    do_upsert_corpus(this, updates.at("updates").as_object());
    EXPECT_EQ(
        this->root_hash(), to_byte_string(updates.at("root_hash").as_string()));
    do_erase_corpus(this, updates.at("updates").as_object());
    EXPECT_EQ(
        this->root_hash(),
        0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421_hex /* null hash */);
}

TYPED_TEST(ManyNestedUpdates, test_corpus_1)
{
    auto const updates = read_corpus("src1").as_object();
    do_upsert_corpus(this, updates.at("updates").as_object());
    EXPECT_EQ(
        this->root_hash(), to_byte_string(updates.at("root_hash").as_string()));
    do_erase_corpus(this, updates.at("updates").as_object());
    EXPECT_EQ(
        this->root_hash(),
        0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421_hex /* null hash */);
}
