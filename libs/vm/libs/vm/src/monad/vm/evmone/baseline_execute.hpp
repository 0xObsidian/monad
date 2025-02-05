#pragma once

#include <monad/vm/evmone/code_analysis.hpp>

#include <evmc/evmc.hpp>

namespace monad
{
    evmc::Result baseline_execute(
        evmc_message const &msg, evmc_revision const rev,
        evmc::Host *const host, CodeAnalysis const &code_analysis);
}
