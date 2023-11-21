#include "gtest/gtest.h"

#include <monad/mpt/detail/unsigned_20.hpp>

TEST(unsigned_20, works)
{
    using MONAD_MPT_NAMESPACE::detail::unsigned_20;

    unsigned_20 a(5), b(6);
    EXPECT_EQ(b - a, 1);
    EXPECT_EQ(a - b, 0xfffff);

    a |= 0xffffffff;
    EXPECT_EQ(a, 0xfffff);
    a += 1;
    EXPECT_EQ(a, 0);

    a = 1 << 19;
    EXPECT_EQ(a, 1 << 19);
    a <<= 1;
    EXPECT_EQ(a, 0);

    a = 0;
    a -= 1;
    EXPECT_EQ(a, 0xfffff);

    // Make sure this follows C's deeply unhelpful integer promotion rules
    static_assert(std::is_same_v<decltype(a + 1), int>);
    static_assert(std::is_same_v<decltype(a + 1U), unsigned>);
    static_assert(std::is_same_v<decltype(a + int16_t(1)), unsigned_20>);
    static_assert(std::is_same_v<decltype(a + uint16_t(1)), unsigned_20>);
}
