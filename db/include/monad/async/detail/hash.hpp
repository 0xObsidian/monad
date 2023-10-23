#pragma once

#include "../config.hpp"

#include <cstddef>

namespace monad
{
    //! \brief A STL compatible hash based on the high quality FNV1 hash
    //! algorithm
    template <class T>
    struct fnv1a_hash
    {
        using working_type = uint64_t;
        static constexpr working_type begin() noexcept
        {
            return 14695981039346656037ULL;
        }
        static constexpr void add(working_type &hash, T v) noexcept
        {
            static constexpr working_type prime = 1099511628211ULL;
            const unsigned char *_v = (const unsigned char *)&v;
            for (size_t n = 0; n < sizeof(T); n++) {
                hash ^= (working_type)_v[n];
                hash *= prime;
            }
        }
        constexpr size_t operator()(T v) const noexcept
        {
            auto ret = begin();
            add(ret, v);
            return ret;
        }
    };
}
