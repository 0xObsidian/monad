#include <monad/mem/allocators.hpp>

#include <monad/config.hpp>
#include <monad/test/gtest_signal_stacktrace_printer.hpp> // NOLINT

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>

namespace
{
    static size_t constructed{0}, destructed{0}, allocated(0), deallocated(0);

    void reset()
    {
        constructed = destructed = allocated = deallocated = 0;
    }

    struct Foo
    {
        int x;

        Foo()
        {
            constructed++;
        }

        Foo(Foo const &o)
            : x(o.x)
        {
            constructed++;
        }

        explicit Foo(int a)
            : x(a)
        {
            constructed++;
        }

        ~Foo()
        {
            destructed++;
        }
    };

    struct custom_allocator
    {
        using value_type = Foo;
        using size_type = size_t;

        [[nodiscard]] value_type *allocate(size_type n)
        {
            allocated++;
            return std::allocator<value_type>().allocate(n);
        }

        void deallocate(value_type *p, size_type n) noexcept
        {
            deallocated++;
            std::allocator<value_type>().deallocate(p, n);
        }

        static custom_allocator &get()
        {
            static custom_allocator v;
            return v;
        }
    };

    struct raw_allocator
    {
        using value_type = std::byte;
        using size_type = size_t;

        [[nodiscard]] value_type *allocate(size_type n)
        {
            allocated++;
            return (std::byte *)std::malloc(n);
        }

        void deallocate(value_type *p, size_type) noexcept
        {
            deallocated++;
            free(p);
        }
    };

    static MONAD_NAMESPACE::allocators::detail::type_raw_alloc_pair<
        custom_allocator, raw_allocator>
    get_type_raw_alloc_pair()
    {
        static custom_allocator a;
        static raw_allocator b;
        return {a, b};
    }

    TEST(AllocatorsTest, allocate_unique)
    {
        using namespace MONAD_NAMESPACE::allocators;
        reset();
        allocate_unique<custom_allocator, &custom_allocator::get>();
        EXPECT_EQ(allocated, 1);
        EXPECT_EQ(constructed, 1);
        EXPECT_EQ(destructed, 1);
        EXPECT_EQ(deallocated, 1);
    }

    TEST(AllocatorsTest, aliasing_unique_ptr)
    {
        using namespace MONAD_NAMESPACE::allocators;
        reset();
        allocate_aliasing_unique<
            custom_allocator,
            raw_allocator,
            &get_type_raw_alloc_pair>(16);
        EXPECT_EQ(allocated, 1);
        EXPECT_EQ(constructed, 1);
        EXPECT_EQ(destructed, 1);
        EXPECT_EQ(deallocated, 1);
        make_aliasing_unique<Foo>(16);
        EXPECT_EQ(allocated, 1);
        EXPECT_EQ(constructed, 2);
        EXPECT_EQ(destructed, 2);
        EXPECT_EQ(deallocated, 1);
    }

    TEST(AllocatorsTest, resizeable_unique_ptr)
    {
        using namespace MONAD_NAMESPACE::allocators;
        auto v = make_resizeable_unique_for_overwrite<int[]>(5);
        v[0] = 2;
        v.resize(1);
        EXPECT_EQ(v[0], 2);
        v.resize(10);
        EXPECT_EQ(v[0], 2);
    }

    TEST(AllocatorsTest, owning_span)
    {
        using namespace MONAD_NAMESPACE::allocators;
        {
            auto v = owning_span<int>(5, 2);
            ASSERT_EQ(v.size(), 5);
            for (auto const &i : v) {
                EXPECT_EQ(i, 2);
            }
        }
        reset();
        {
            auto v = owning_span<Foo, custom_allocator>(5, Foo(2));
            ASSERT_EQ(v.size(), 5);
            for (auto const &i : v) {
                EXPECT_EQ(i.x, 2);
            }
        }
        EXPECT_EQ(allocated, 1);
        EXPECT_EQ(constructed, 6);
        EXPECT_EQ(destructed, 6);
        EXPECT_EQ(deallocated, 1);
    }

    TEST(AllocatorsTest, thread_local_delayed_unique_ptr_resetter)
    {
        using namespace MONAD_NAMESPACE::allocators;
        reset();
        {
            using type = std::unique_ptr<Foo>;
            thread_local_delayed_unique_ptr_resetter<type> const resetter;
            {
                type x = std::make_unique<Foo>();
                EXPECT_EQ(constructed, 1);
                EXPECT_EQ(destructed, 0);
                delayed_reset(std::move(x));
                EXPECT_EQ(constructed, 1);
                EXPECT_EQ(destructed, 0);
            }
            EXPECT_EQ(constructed, 1);
            EXPECT_EQ(destructed, 0);
        }
        EXPECT_EQ(constructed, 1);
        EXPECT_EQ(destructed, 1);
    }

#if MONAD_CORE_ALLOCATORS_HAVE_BOOST_POOL &&                                   \
    !MONAD_CORE_ALLOCATORS_DISABLE_BOOST_OBJECT_POOL
    TEST(AllocatorsTest, array_of_boost_pools_allocator)
    {
        using namespace MONAD_NAMESPACE::allocators;
        using allocator_type = array_of_boost_pools_allocator<8, 64, 4>;
        allocator_type alloc;
        auto *const p1 = alloc.allocate(16);
        auto *const p2 = alloc.allocate(16);
        auto *const p3 = alloc.allocate(24);
        auto *const p4 = alloc.allocate(24);
        EXPECT_EQ(p2, p1 + 16);
        EXPECT_EQ(p4, p3 + 24);
        alloc.deallocate(p1, 16);
        alloc.deallocate(p2, 16);
        alloc.deallocate(p3, 24);
        alloc.deallocate(p4, 24);

        auto *const p5 = alloc.allocate(64);
        EXPECT_TRUE(p5 != nullptr);
        if (p5) {
            alloc.deallocate(p5, 64);
        }

        EXPECT_THROW((void)alloc.allocate(65), std::invalid_argument);
    }
#endif
}
