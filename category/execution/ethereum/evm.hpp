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

#pragma once

#include <category/core/config.hpp>
#include <category/execution/ethereum/core/address.hpp>

#include <evmc/evmc.h>
#include <evmc/evmc.hpp>

MONAD_NAMESPACE_BEGIN

template <evmc_revision rev>
struct EvmcHost;

class State;

template <evmc_revision rev>
evmc::Result deploy_contract_code(
    State &, Address const &, evmc::Result, size_t max_code_size) noexcept;

template <evmc_revision rev>
evmc::Result create(
    EvmcHost<rev> *, State &, evmc_message const &,
    size_t max_code_size);

template <evmc_revision rev>
evmc::Result call(EvmcHost<rev> *, State &, evmc_message const &);

MONAD_NAMESPACE_END
