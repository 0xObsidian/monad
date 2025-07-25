#include <category/core/config.hpp>
#include <category/core/int.hpp>
#include <category/core/likely.h>
#include <category/execution/monad/chain/monad_revision.h>
#include <category/execution/monad/chain/monad_testnet2.hpp>
#include <category/execution/monad/chain/monad_testnet2_alloc.hpp>

MONAD_NAMESPACE_BEGIN

monad_revision MonadTestnet2::get_monad_revision(
    uint64_t /* block_number */, uint64_t const timestamp) const
{
    if (MONAD_LIKELY(timestamp >= 1753795800)) { // 2025-07-29T13:30:00.000Z
        return MONAD_THREE;
    }
    return MONAD_TWO;
}

uint256_t MonadTestnet2::get_chain_id() const
{
    return 30143;
};

GenesisState MonadTestnet2::get_genesis_state() const
{
    BlockHeader header;
    header.gas_limit = 5000;
    header.extra_data = evmc::from_hex("0x0000000000000000000000000000000000000"
                                       "000000000000000000000000000")
                            .value();
    header.base_fee_per_gas = 0;
    header.withdrawals_root = NULL_ROOT;
    header.blob_gas_used = 0;
    header.excess_blob_gas = 0;
    header.parent_beacon_block_root = NULL_ROOT;
    return {header, MONAD_TESTNET2_ALLOC};
}

MONAD_NAMESPACE_END
