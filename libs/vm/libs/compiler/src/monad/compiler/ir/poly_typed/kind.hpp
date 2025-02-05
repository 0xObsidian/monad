#pragma once

#include <monad/compiler/transactional_unordered_map.hpp>

#include <format>
#include <memory>
#include <variant>
#include <vector>

namespace monad::compiler::poly_typed
{
    using VarName = uint64_t;

    struct Word;
    struct Any;
    struct KindVar;
    struct LiteralVar;
    struct WordCont;
    struct Cont;

    using PreKind =
        std::variant<Word, Any, KindVar, LiteralVar, WordCont, Cont>;

    using Kind = std::shared_ptr<PreKind>;

    struct ContVar
    {
        VarName var;
    };

    struct ContWords
    {
    };

    using ContTailKind = std::variant<ContVar, ContWords>;

    struct PreContKind
    {
        std::vector<Kind> front;
        ContTailKind tail;
    };

    using ContKind = std::shared_ptr<PreContKind>;

    extern ContKind cont_words;

    ContKind cont_kind(std::vector<Kind> kinds, ContTailKind t);

    ContKind cont_kind(std::vector<Kind> kinds, VarName v);

    ContKind cont_kind(std::vector<Kind> kinds);

    struct Word
    {
    };

    extern Kind word;

    struct Any
    {
    };

    extern Kind any;

    struct KindVar
    {
        VarName var;
    };

    Kind kind_var(VarName);

    struct LiteralVar
    {
        VarName var;
        ContKind cont;
    };

    Kind literal_var(VarName, ContKind);

    struct WordCont
    {
        ContKind cont;
    };

    Kind word_cont(ContKind);

    struct Cont
    {
        ContKind cont;
    };

    Kind cont(ContKind);

    struct PolyVarSubstMap
    {
        std::unordered_map<VarName, VarName> kind_map;
        std::unordered_map<VarName, VarName> cont_map;
    };

    enum class LiteralType
    {
        Word,
        Cont,
        WordCont
    };

    inline bool operator==(LiteralType t1, LiteralType t2)
    {
        return static_cast<int>(t1) == static_cast<int>(t2);
    }

    inline bool operator!=(LiteralType t1, LiteralType t2)
    {
        return !(t1 == t2);
    }

    void
    format_kind(Kind const &kind, std::format_context &ctx, bool use_parens);

    void format_cont(ContKind const &cont, std::format_context &ctx);

    /// Equality up to renaming of variables.
    /// Does not consider Word.. to be equal to Word,Word..
    bool alpha_equal(Kind, Kind);

    bool operator==(Kind, Kind) = delete;
    bool operator!=(Kind, Kind) = delete;

    /// Equality where Word.. is equal to Word,Word..
    bool weak_equal(Kind, Kind);

    /// Whether there exists a `SubstMap su`, such that `su.subst(generic) ==
    /// specific`. The function considers Word.. to be equal to Word,Word..
    bool can_specialize(Kind generic, Kind specific);

    /// Equality up to renaming of variables.
    /// Does not consider Word.. to be equal to Word,Word..
    bool alpha_equal(ContKind, ContKind);

    bool operator==(ContKind, ContKind) = delete;
    bool operator!=(ContKind, ContKind) = delete;

    /// Equality where Word.. is equal to Word,Word..
    bool weak_equal(ContKind, ContKind);

    /// Whether there exists a `SubstMap su`, such that `su.subst(generic) ==
    /// specific`. The function considers Word.. to be equal to Word,Word..
    bool can_specialize(ContKind generic, ContKind specific);
}

template <>
struct std::formatter<monad::compiler::poly_typed::Kind>
{
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(
        monad::compiler::poly_typed::Kind const &kind,
        std::format_context &ctx) const
    {
        format_kind(kind, ctx, false);
        return ctx.out();
    }
};

template <>
struct std::formatter<monad::compiler::poly_typed::ContKind>
{
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(
        monad::compiler::poly_typed::ContKind const &kind,
        std::format_context &ctx) const
    {
        format_cont(kind, ctx);
        return ctx.out();
    }
};
