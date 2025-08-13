// Copyright (C) 2025 Category Labs, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <category/core/byte_string.hpp>
#include <category/core/config.hpp>
#include <category/core/keccak.hpp>
#include <category/execution/ethereum/core/address.hpp>
#include <category/execution/ethereum/core/rlp/transaction_rlp.hpp>
#include <category/execution/ethereum/core/transaction.hpp>
#include <category/execution/ethereum/trace/event_trace.hpp>

#include <silkpre/ecdsa.h>

#include <ethash/hash_types.hpp>

#include <intx/intx.hpp>

#include <secp256k1.h>

#include <cstdint>
#include <memory>
#include <optional>

MONAD_NAMESPACE_BEGIN

std::optional<Address> recover_sender(Transaction const &tx)
{
    if (tx.sc.y_parity > 1) {
        return std::nullopt;
    }

    TRACE_TXN_EVENT(StartSenderRecovery);
    byte_string const tx_encoding = rlp::encode_transaction_for_signing(tx);
    auto const tx_encoding_hash = keccak256(tx_encoding);

    uint8_t signature[sizeof(tx.sc.r) * 2];
    intx::be::unsafe::store(signature, tx.sc.r);
    intx::be::unsafe::store(signature + sizeof(tx.sc.r), tx.sc.s);

    thread_local std::unique_ptr<
        secp256k1_context,
        decltype(&secp256k1_context_destroy)> const
        context(
            secp256k1_context_create(SILKPRE_SECP256K1_CONTEXT_FLAGS),
            &secp256k1_context_destroy);

    Address result;

    if (!silkpre_recover_address(
            result.bytes,
            tx_encoding_hash.bytes,
            signature,
            tx.sc.y_parity,
            context.get())) {
        return std::nullopt;
    }

    return result;
}

MONAD_NAMESPACE_END
