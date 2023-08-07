#pragma once

#include <monad/db/config.hpp>
#include <monad/db/db_interface.hpp>
#include <monad/execution/execution_model.hpp>

MONAD_DB_NAMESPACE_BEGIN

namespace detail
{
    // Database impl without trie root generating logic, backed by stl
    template <typename TExecutor, Permission TPermission>
    struct InMemoryDB
        : public DBInterface<
              InMemoryDB<TExecutor, TPermission>, TExecutor, TPermission>
    {
        using base_t = DBInterface<
            InMemoryDB<TExecutor, TPermission>, TExecutor, TPermission>;

        std::unordered_map<address_t, Account> accounts;
        std::unordered_map<address_t, std::unordered_map<bytes32_t, bytes32_t>>
            storage;

        ////////////////////////////////////////////////////////////////////
        // DBInterface implementations
        ////////////////////////////////////////////////////////////////////

        [[nodiscard]] bool contains_impl(address_t const &a)
            requires Readable<TPermission>
        {
            return accounts.contains(a);
        }

        [[nodiscard]] bool contains_impl(address_t const &a, bytes32_t const &k)
            requires Readable<TPermission>
        {
            return storage.contains(a) && storage.at(a).contains(k);
        }

        [[nodiscard]] std::optional<Account> try_find_impl(address_t const &a)
            requires Readable<TPermission>
        {
            if (accounts.contains(a)) {
                return accounts.at(a);
            }
            return std::nullopt;
        }

        [[nodiscard]] bytes32_t
        try_find_impl(address_t const &a, bytes32_t const &k)
            requires Readable<TPermission>
        {
            if (!base_t::contains(a, k)) {
                return bytes32_t{};
            }
            return storage.at(a).at(k);
        }

        void commit(state::changeset auto const &obj)
            requires Writable<TPermission>
        {
            for (auto const &[a, updates] : obj.storage_changes) {
                for (auto const &[k, v] : updates) {
                    if (v != bytes32_t{}) {
                        storage[a][k] = v;
                    }
                    else {
                        auto const n = storage[a].erase(k);
                        MONAD_DEBUG_ASSERT(n == 1);
                    }
                }
                if (storage[a].empty()) {
                    storage.erase(a);
                }
            }

            for (auto const &[a, acct] : obj.account_changes) {
                if (acct.has_value()) {
                    accounts[a] = acct.value();
                }
                else {
                    auto const n = accounts.erase(a);
                    MONAD_DEBUG_ASSERT(n == 1);
                }
            }
        }

        constexpr void
        create_and_prune_block_history(uint64_t /* block_number */) const
            requires Writable<TPermission>
        {
        }
    };
}

using InMemoryDB =
    detail::InMemoryDB<monad::execution::SerialExecution, ReadWrite>;

MONAD_DB_NAMESPACE_END
