#pragma once

#include <monad/config.hpp>

#include <quill/Quill.h>
#include <quill/bundled/fmt/format.h>
#include <quill/bundled/fmt/ranges.h>

namespace fmt = fmtquill::v10;

MONAD_NAMESPACE_BEGIN

struct basic_formatter
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx)
    {
        return ctx.begin();
    }
};

MONAD_NAMESPACE_END
