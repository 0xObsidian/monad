#include <monad/rlp/encode.hpp>

#include <monad/core/byte_string.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>

using monad::byte_string;
using monad::byte_string_view;

TEST(rlp, impl_length_length)
{
    size_t result;

    result = monad::rlp::impl::length_length(0);
    EXPECT_EQ(result, 0);

    result = monad::rlp::impl::length_length(1);
    EXPECT_EQ(result, 1);

    result = monad::rlp::impl::length_length(255);
    EXPECT_EQ(result, 1);

    result = monad::rlp::impl::length_length(256);
    EXPECT_EQ(result, 2);

    result = monad::rlp::impl::length_length(65535);
    EXPECT_EQ(result, 2);

    result = monad::rlp::impl::length_length(65536);
    EXPECT_EQ(result, 3);

    result = monad::rlp::impl::length_length((1UL << 56) - 1);
    EXPECT_EQ(result, 7);

    result = monad::rlp::impl::length_length(1UL << 56);
    EXPECT_EQ(result, 8);

    result = monad::rlp::impl::length_length(0xFFFFFFFFFFFFFFFFUL);
    EXPECT_EQ(result, 8);
}

TEST(rlp, impl_encode_length)
{
    unsigned char buf[8];
    unsigned char *result;

    result = monad::rlp::impl::encode_length(buf, 0);
    EXPECT_EQ(result - buf, 0);

    result = monad::rlp::impl::encode_length(buf, 1);
    EXPECT_EQ(result - buf, 1);
    EXPECT_TRUE(std::equal(buf, result, (unsigned char[]){1}));

    result = monad::rlp::impl::encode_length(buf, 255);
    EXPECT_EQ(result - buf, 1);
    EXPECT_TRUE(std::equal(buf, result, (unsigned char[]){255}));

    result = monad::rlp::impl::encode_length(buf, 256);
    EXPECT_EQ(result - buf, 2);
    EXPECT_TRUE(std::equal(buf, result, (unsigned char[]){1, 0}));

    result = monad::rlp::impl::encode_length(buf, 258);
    EXPECT_EQ(result - buf, 2);
    EXPECT_TRUE(std::equal(buf, result, (unsigned char[]){1, 2}));

    result = monad::rlp::impl::encode_length(buf, 0xFFFFFFFFFFFFFFFFUL);
    EXPECT_EQ(result - buf, 8);
    EXPECT_TRUE(std::equal(
        buf,
        result,
        (unsigned char[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}));
}

TEST(rlp, string_length)
{
    size_t result;

    result = monad::rlp::string_length(
        monad::to_byte_string_view((unsigned char[]){1}));
    EXPECT_EQ(result, 1);

    result = monad::rlp::string_length(
        monad::to_byte_string_view((unsigned char[]){128}));
    EXPECT_EQ(result, 2);

    result = monad::rlp::string_length({});
    EXPECT_EQ(result, 1);

    result = monad::rlp::string_length(
        monad::to_byte_string_view((unsigned char[]){1, 2}));
    EXPECT_EQ(result, 3);

    result = monad::rlp::string_length(byte_string(55, 1));
    EXPECT_EQ(result, 56);

    result = monad::rlp::string_length(byte_string(56, 1));
    EXPECT_EQ(result, 58);
}

TEST(rlp, encode_string)
{
    unsigned char buf[256];
    unsigned char *result;

    result = monad::rlp::encode_string(buf, byte_string({1}));
    EXPECT_EQ(result - buf, 1);
    EXPECT_TRUE(byte_string_view(buf, result) == byte_string({1}));

    result = monad::rlp::encode_string(buf, byte_string({128}));
    EXPECT_EQ(result - buf, 2);
    EXPECT_TRUE(byte_string_view(buf, result) == byte_string({129, 128}));

    result = monad::rlp::encode_string(buf, byte_string{});
    EXPECT_EQ(result - buf, 1);
    EXPECT_TRUE(byte_string_view(buf, result) == byte_string({128}));

    result = monad::rlp::encode_string(buf, byte_string({1, 2}));
    EXPECT_EQ(result - buf, 3);
    EXPECT_TRUE(byte_string_view(buf, result) == byte_string({130, 1, 2}));

    result = monad::rlp::encode_string(buf, byte_string(55, 1));
    EXPECT_EQ(result - buf, 56);
    EXPECT_TRUE(
        byte_string_view(buf, result) ==
        byte_string({183}) + byte_string(55, 1));

    result = monad::rlp::encode_string(buf, byte_string(56, 1));
    EXPECT_EQ(result - buf, 58);
    EXPECT_TRUE(
        byte_string_view(buf, result) ==
        byte_string({184, 56}) + byte_string(56, 1));
}
