#include <monad/vm/core/assert.h>
#include <monad/vm/utils/uint256.hpp>

#include <intx/intx.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace monad::vm::utils
{
    uint256_t signextend(uint256_t const &byte_index_256, uint256_t const &x)
    {
        if (byte_index_256 >= 31) {
            return x;
        }
        uint64_t const byte_index = byte_index_256[0];
        uint64_t const word_index = byte_index >> 3;
        uint64_t const word = x[word_index];
        int64_t const signed_word = static_cast<int64_t>(word);
        uint64_t const bit_index = (byte_index & 7) * 8;
        int64_t const signed_byte = static_cast<int8_t>(word >> bit_index);
        uint64_t const upper = static_cast<uint64_t>(signed_byte) << bit_index;
        int64_t const signed_lower =
            signed_word &
            ~(std::numeric_limits<int64_t>::min() >> (63 - bit_index));
        uint64_t const lower = static_cast<uint64_t>(signed_lower);
        uint64_t const sign_bits = static_cast<uint64_t>(signed_byte >> 63);
        uint256_t ret;
        for (uint64_t j = 0; j < word_index; ++j) {
            ret[j] = x[j];
        }
        ret[word_index] = upper | lower;
        for (uint64_t j = word_index + 1; j < 4; ++j) {
            ret[j] = sign_bits;
        }
        return ret;
    }

    uint256_t byte(uint256_t const &byte_index_256, uint256_t const &x)
    {
        if (byte_index_256 >= 32) {
            return 0;
        }
        uint64_t const byte_index = 31 - byte_index_256[0];
        uint64_t const word_index = byte_index >> 3;
        uint64_t const word = x[word_index];
        uint64_t const bit_index = (byte_index & 7) * 8;
        uint64_t const byte = static_cast<uint8_t>(word >> bit_index);
        uint256_t ret{0};
        ret[0] = byte;
        return ret;
    }

    uint256_t sar(uint256_t const &shift_index_256, uint256_t const &x)
    {
        int64_t shift_index = shift_index_256 >= 255
                                  ? 255
                                  : static_cast<int64_t>(shift_index_256[0]);
        uint64_t const sign_bit =
            x[3] & static_cast<uint64_t>(std::numeric_limits<int64_t>::min());
        uint256_t sign_bits{0};
        size_t i = 4;
        while (shift_index > 0) {
            uint64_t const shift =
                std::min(uint64_t{63}, static_cast<uint64_t>(shift_index));
            sign_bits[--i] =
                static_cast<uint64_t>(static_cast<int64_t>(sign_bit) >> shift);
            shift_index -= 64;
        }
        return (x >> shift_index_256) | sign_bits;
    }

    uint256_t countr_zero(uint256_t const &x)
    {
        int total_count = 0;
        for (size_t i = 0; i < 4; i++) {
            int const count = std::countr_zero(x[i]);
            total_count += count;
            if (count < 64) {
                return uint256_t{total_count};
            }
        }
        return uint256_t{total_count};
    }

    uint256_t
    from_bytes(std::size_t n, std::size_t remaining, uint8_t const *src)
    {
        MONAD_VM_ASSERT(n <= 32);

        if (n == 0) {
            return 0;
        }

        uint8_t dst[32] = {};

        std::memcpy(&dst[32 - n], src, std::min(n, remaining));

        return uint256_t::load_be(dst);
    }

    uint256_t from_bytes(std::size_t const n, uint8_t const *src)
    {
        return from_bytes(n, n, src);
    }
}
