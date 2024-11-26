#pragma once

#include <utils/assert.h>

#include <evmc/evmc.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string_view>
#include <tuple>

namespace monad::compiler
{
    /**
     * Details of how an individual EVM opcode affects VM state when executed.
     */
    struct OpCodeInfo
    {
        /**
         * The human-readable (disassembled) form of the opcode.
         */
        std::string_view name;

        /**
         * The number of argument bytes that follow this opcode in a binary EVM
         * program.
         *
         * This value is 0 for all instructions other than the `PUSHN` family,
         * each of which expects N bytes to follow.
         */
        std::uint8_t num_args;

        /**
         * The minimum EVM stack size required to execute this instruction.
         */
        std::uint8_t min_stack;

        /**
         * Whether the EVM stack size increases after executing this
         * instruction.
         */
        bool increases_stack;

        /**
         * Whether the gas cost of this instruction is determined at runtime.
         */
        bool dynamic_gas;

        /**
         * Minimum static gas required to execute this instruction.
         *
         * Some instructions may also consume additional dynamic gas depending
         * on run-time properties (e.g. memory expansion or storage costs).
         */
        std::uint16_t min_gas;

        /**
         * The index within a set of related opcodes for this instruction.
         *
         * N for all PUSHN, SWAPN, DUPN and LOGN instructions, and 0 otherwise.
         */
        std::uint8_t index;
    };

    constexpr bool operator==(OpCodeInfo const &a, OpCodeInfo const &b)
    {
        return std::tie(
                   a.name,
                   a.num_args,
                   a.min_stack,
                   a.increases_stack,
                   a.min_gas) ==
               std::tie(
                   b.name,
                   b.num_args,
                   b.min_stack,
                   b.increases_stack,
                   b.min_gas);
    }

    consteval evmc_revision previous_evm_revision(evmc_revision rev)
    {
        MONAD_COMPILER_DEBUG_ASSERT(rev > EVMC_FRONTIER);
        return evmc_revision(std::to_underlying(rev) - 1);
    }

    /**
     * Placeholder value representing an opcode value not currently used by the
     * EVM specification. The value of `unknown_opcode_info` is significant, so
     * cannot a-priori be changed.
     */
    inline constexpr auto unknown_opcode_info =
        OpCodeInfo{"UNKNOWN", 0, 0, false, false, 0, 0};

    /**
     * Lookup table of opcode info for each possible 1-byte opcode value.
     *
     * Some bytes do not correspond to an EVM instruction; looking those bytes
     * up in this table produces a placeholder value. This depends additionally
     * on the specified EVM revision (that is, some opcodes are invalid in early
     * revisions and become valid in later ones).
     */
    template <evmc_revision Rev>
    consteval std::array<OpCodeInfo, 256> opcode_table()
    {
        return opcode_table<previous_evm_revision(Rev)>();
    }

    consteval inline void add_opcode(
        std::uint8_t opcode, std::array<OpCodeInfo, 256> &table,
        OpCodeInfo info)
    {
        MONAD_COMPILER_DEBUG_ASSERT(table[opcode] == unknown_opcode_info);
        table[opcode] = info;
    }

    template <>
    consteval std::array<OpCodeInfo, 256> opcode_table<EVMC_FRONTIER>()
    {
        return {
            OpCodeInfo{"STOP", 0, 0, false, false, 0, 0}, // 0x00
            OpCodeInfo{"ADD", 0, 2, true, false, 3, 0}, // 0x01
            OpCodeInfo{"MUL", 0, 2, true, false, 5, 0}, // 0x02
            OpCodeInfo{"SUB", 0, 2, true, false, 3, 0}, // 0x03
            OpCodeInfo{"DIV", 0, 2, true, false, 5, 0}, // 0x04,
            OpCodeInfo{"SDIV", 0, 2, true, false, 5, 0}, // 0x05,
            OpCodeInfo{"MOD", 0, 2, true, false, 5, 0}, // 0x06,
            OpCodeInfo{"SMOD", 0, 2, true, false, 5, 0}, // 0x07,
            OpCodeInfo{"ADDMOD", 0, 3, true, false, 8, 0}, // 0x08,
            OpCodeInfo{"MULMOD", 0, 3, true, false, 8, 0}, // 0x09,
            OpCodeInfo{"EXP", 0, 2, true, true, 10, 0}, // 0x0A,
            OpCodeInfo{"SIGNEXTEND", 0, 2, true, false, 5, 0}, // 0x0B,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            OpCodeInfo{"LT", 0, 2, true, false, 3, 0}, // 0x10,
            OpCodeInfo{"GT", 0, 2, true, false, 3, 0}, // 0x11,
            OpCodeInfo{"SLT", 0, 2, true, false, 3, 0}, // 0x12,
            OpCodeInfo{"SGT", 0, 2, true, false, 3, 0}, // 0x13,
            OpCodeInfo{"EQ", 0, 2, true, false, 3, 0}, // 0x14,
            OpCodeInfo{"ISZERO", 0, 1, true, false, 3, 0}, // 0x15,
            OpCodeInfo{"AND", 0, 2, true, false, 3, 0}, // 0x16,
            OpCodeInfo{"OR", 0, 2, true, false, 3, 0}, // 0x17,
            OpCodeInfo{"XOR", 0, 2, true, false, 3, 0}, // 0x18,
            OpCodeInfo{"NOT", 0, 1, true, false, 3, 0}, // 0x19,
            OpCodeInfo{"BYTE", 0, 2, true, false, 3, 0}, // 0x1A,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            OpCodeInfo{"SHA3", 0, 2, true, true, 30, 0}, // 0x20,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            OpCodeInfo{"ADDRESS", 0, 0, true, false, 2, 0}, // 0x30,
            OpCodeInfo{"BALANCE", 0, 1, true, true, 100, 0}, // 0x31,
            OpCodeInfo{"ORIGIN", 0, 0, true, false, 2, 0}, // 0x32,
            OpCodeInfo{"CALLER", 0, 0, true, false, 2, 0}, // 0x33,
            OpCodeInfo{"CALLVALUE", 0, 0, true, false, 2, 0}, // 0x34,
            OpCodeInfo{"CALLDATALOAD", 0, 1, true, false, 3, 0}, // 0x35,
            OpCodeInfo{"CALLDATASIZE", 0, 0, true, false, 2, 0}, // 0x36,
            OpCodeInfo{"CALLDATACOPY", 0, 3, false, true, 3, 0}, // 0x37,
            OpCodeInfo{"CODESIZE", 0, 0, true, false, 2, 0}, // 0x38,
            OpCodeInfo{"CODECOPY", 0, 3, false, true, 3, 0}, // 0x39,
            OpCodeInfo{"GASPRICE", 0, 0, true, false, 2, 0}, // 0x3A,
            OpCodeInfo{"EXTCODESIZE", 0, 1, true, true, 100, 0}, // 0x3B,
            OpCodeInfo{"EXTCODECOPY", 0, 4, false, true, 100, 0}, // 0x3C,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            OpCodeInfo{"BLOCKHASH", 0, 1, true, false, 20, 0}, // 0x40,
            OpCodeInfo{"COINBASE", 0, 0, true, false, 2, 0}, // 0x41,
            OpCodeInfo{"TIMESTAMP", 0, 0, true, false, 2, 0}, // 0x42,
            OpCodeInfo{"NUMBER", 0, 0, true, false, 2, 0}, // 0x43,
            OpCodeInfo{"DIFFICULTY", 0, 0, true, false, 2, 0}, // 0x44,
            OpCodeInfo{"GASLIMIT", 0, 0, true, false, 2, 0}, // 0x45,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            OpCodeInfo{"POP", 0, 1, false, false, 2, 0}, // 0x50,
            OpCodeInfo{"MLOAD", 0, 1, true, true, 3, 0}, // 0x51,
            OpCodeInfo{"MSTORE", 0, 2, false, true, 3, 0}, // 0x52,
            OpCodeInfo{"MSTORE8", 0, 2, false, true, 3, 0}, // 0x53,
            OpCodeInfo{"SLOAD", 0, 1, true, true, 100, 0}, // 0x54,
            OpCodeInfo{"SSTORE", 0, 2, false, true, 100, 0}, // 0x55,
            OpCodeInfo{"JUMP", 0, 1, false, false, 8, 0}, // 0x56,
            OpCodeInfo{"JUMPI", 0, 2, false, false, 10, 0}, // 0x57,
            OpCodeInfo{"PC", 0, 0, true, false, 2, 0}, // 0x58,
            OpCodeInfo{"MSIZE", 0, 0, true, false, 2, 0}, // 0x59,
            OpCodeInfo{"GAS", 0, 0, true, false, 2, 0}, // 0x5A,
            OpCodeInfo{"JUMPDEST", 0, 0, false, false, 1, 0}, // 0x5B,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            OpCodeInfo{"PUSH1", 1, 0, true, false, 3, 1}, // 0x60,
            OpCodeInfo{"PUSH2", 2, 0, true, false, 3, 2}, // 0x61,
            OpCodeInfo{"PUSH3", 3, 0, true, false, 3, 3}, // 0x62,
            OpCodeInfo{"PUSH4", 4, 0, true, false, 3, 4}, // 0x63,
            OpCodeInfo{"PUSH5", 5, 0, true, false, 3, 5}, // 0x64,
            OpCodeInfo{"PUSH6", 6, 0, true, false, 3, 6}, // 0x65,
            OpCodeInfo{"PUSH7", 7, 0, true, false, 3, 7}, // 0x66,
            OpCodeInfo{"PUSH8", 8, 0, true, false, 3, 8}, // 0x67,
            OpCodeInfo{"PUSH9", 9, 0, true, false, 3, 9}, // 0x68,
            OpCodeInfo{"PUSH10", 10, 0, true, false, 3, 10}, // 0x69,
            OpCodeInfo{"PUSH11", 11, 0, true, false, 3, 11}, // 0x6A,
            OpCodeInfo{"PUSH12", 12, 0, true, false, 3, 12}, // 0x6B,
            OpCodeInfo{"PUSH13", 13, 0, true, false, 3, 13}, // 0x6C,
            OpCodeInfo{"PUSH14", 14, 0, true, false, 3, 14}, // 0x6D,
            OpCodeInfo{"PUSH15", 15, 0, true, false, 3, 15}, // 0x6E,
            OpCodeInfo{"PUSH16", 16, 0, true, false, 3, 16}, // 0x6F,

            OpCodeInfo{"PUSH17", 17, 0, true, false, 3, 17}, // 0x70,
            OpCodeInfo{"PUSH18", 18, 0, true, false, 3, 18}, // 0x71,
            OpCodeInfo{"PUSH19", 19, 0, true, false, 3, 19}, // 0x72,
            OpCodeInfo{"PUSH20", 20, 0, true, false, 3, 20}, // 0x73,
            OpCodeInfo{"PUSH21", 21, 0, true, false, 3, 21}, // 0x74,
            OpCodeInfo{"PUSH22", 22, 0, true, false, 3, 22}, // 0x75,
            OpCodeInfo{"PUSH23", 23, 0, true, false, 3, 23}, // 0x76,
            OpCodeInfo{"PUSH24", 24, 0, true, false, 3, 24}, // 0x77,
            OpCodeInfo{"PUSH25", 25, 0, true, false, 3, 25}, // 0x78,
            OpCodeInfo{"PUSH26", 26, 0, true, false, 3, 26}, // 0x79,
            OpCodeInfo{"PUSH27", 27, 0, true, false, 3, 27}, // 0x7A,
            OpCodeInfo{"PUSH28", 28, 0, true, false, 3, 28}, // 0x7B,
            OpCodeInfo{"PUSH29", 29, 0, true, false, 3, 29}, // 0x7C,
            OpCodeInfo{"PUSH30", 30, 0, true, false, 3, 30}, // 0x7D,
            OpCodeInfo{"PUSH31", 31, 0, true, false, 3, 31}, // 0x7E,
            OpCodeInfo{"PUSH32", 32, 0, true, false, 3, 32}, // 0x7F,

            OpCodeInfo{"DUP1", 0, 1, true, false, 3, 1}, // 0x80,
            OpCodeInfo{"DUP2", 0, 2, true, false, 3, 2}, // 0x81,
            OpCodeInfo{"DUP3", 0, 3, true, false, 3, 3}, // 0x82,
            OpCodeInfo{"DUP4", 0, 4, true, false, 3, 4}, // 0x83,
            OpCodeInfo{"DUP5", 0, 5, true, false, 3, 5}, // 0x84,
            OpCodeInfo{"DUP6", 0, 6, true, false, 3, 6}, // 0x85,
            OpCodeInfo{"DUP7", 0, 7, true, false, 3, 7}, // 0x86,
            OpCodeInfo{"DUP8", 0, 8, true, false, 3, 8}, // 0x87,
            OpCodeInfo{"DUP9", 0, 9, true, false, 3, 9}, // 0x88,
            OpCodeInfo{"DUP10", 0, 10, true, false, 3, 10}, // 0x89,
            OpCodeInfo{"DUP11", 0, 11, true, false, 3, 11}, // 0x8A,
            OpCodeInfo{"DUP12", 0, 12, true, false, 3, 12}, // 0x8B,
            OpCodeInfo{"DUP13", 0, 13, true, false, 3, 13}, // 0x8C,
            OpCodeInfo{"DUP14", 0, 14, true, false, 3, 14}, // 0x8D,
            OpCodeInfo{"DUP15", 0, 15, true, false, 3, 15}, // 0x8E,
            OpCodeInfo{"DUP16", 0, 16, true, false, 3, 16}, // 0x8F,

            OpCodeInfo{"SWAP1", 0, 1 + 1, false, false, 3, 1}, // 0x90,
            OpCodeInfo{"SWAP2", 0, 1 + 2, false, false, 3, 2}, // 0x91,
            OpCodeInfo{"SWAP3", 0, 1 + 3, false, false, 3, 3}, // 0x92,
            OpCodeInfo{"SWAP4", 0, 1 + 4, false, false, 3, 4}, // 0x93,
            OpCodeInfo{"SWAP5", 0, 1 + 5, false, false, 3, 5}, // 0x94,
            OpCodeInfo{"SWAP6", 0, 1 + 6, false, false, 3, 6}, // 0x95,
            OpCodeInfo{"SWAP7", 0, 1 + 7, false, false, 3, 7}, // 0x96,
            OpCodeInfo{"SWAP8", 0, 1 + 8, false, false, 3, 8}, // 0x97,
            OpCodeInfo{"SWAP9", 0, 1 + 9, false, false, 3, 9}, // 0x98,
            OpCodeInfo{"SWAP10", 0, 1 + 10, false, false, 3, 10}, // 0x99,
            OpCodeInfo{"SWAP11", 0, 1 + 11, false, false, 3, 11}, // 0x9A,
            OpCodeInfo{"SWAP12", 0, 1 + 12, false, false, 3, 12}, // 0x9B,
            OpCodeInfo{"SWAP13", 0, 1 + 13, false, false, 3, 13}, // 0x9C,
            OpCodeInfo{"SWAP14", 0, 1 + 14, false, false, 3, 14}, // 0x9D,
            OpCodeInfo{"SWAP15", 0, 1 + 15, false, false, 3, 15}, // 0x9E,
            OpCodeInfo{"SWAP16", 0, 1 + 16, false, false, 3, 16}, // 0x9F,

            OpCodeInfo{"LOG0", 0, 2 + 0, false, true, 375, 0}, // 0xA0,
            OpCodeInfo{"LOG1", 0, 2 + 1, false, true, 750, 1}, // 0xA1,
            OpCodeInfo{"LOG2", 0, 2 + 2, false, true, 1125, 2}, // 0xA2,
            OpCodeInfo{"LOG3", 0, 2 + 3, false, true, 1500, 3}, // 0xA3,
            OpCodeInfo{"LOG4", 0, 2 + 4, false, true, 1875, 4}, // 0xA4,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            unknown_opcode_info, // 0xB0
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            unknown_opcode_info, // 0xC0
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            unknown_opcode_info, // 0xD0
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            unknown_opcode_info, // 0xE0
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,

            OpCodeInfo{"CREATE", 0, 3, true, true, 32000, 0}, // 0xF0,
            OpCodeInfo{"CALL", 0, 7, true, true, 100, 0}, // 0xF1,
            OpCodeInfo{"CALLCODE", 0, 7, true, true, 100, 0}, // 0xF2,
            OpCodeInfo{"RETURN", 0, 2, false, true, 0, 0}, // 0xF3,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            unknown_opcode_info,
            OpCodeInfo{"SELFDESTRUCT", 0, 1, false, true, 5000, 0} // 0xFF,
        };
    }

    template <>
    consteval std::array<OpCodeInfo, 256> opcode_table<EVMC_BYZANTIUM>()
    {
        auto table = opcode_table<previous_evm_revision(EVMC_BYZANTIUM)>();

        add_opcode(0x3D, table, {"RETURNDATASIZE", 0, 0, true, false, 2, 0});
        add_opcode(0x3E, table, {"RETURNDATACOPY", 0, 3, false, true, 3, 0});
        add_opcode(0xF4, table, {"DELEGATECALL", 0, 6, true, true, 100, 0});
        add_opcode(0xFA, table, {"STATICCALL", 0, 6, true, true, 100, 0});
        add_opcode(0xFD, table, {"REVERT", 0, 2, false, true, 0, 0});

        return table;
    }

    template <>
    consteval std::array<OpCodeInfo, 256> opcode_table<EVMC_CONSTANTINOPLE>()
    {
        auto table = opcode_table<previous_evm_revision(EVMC_CONSTANTINOPLE)>();

        add_opcode(0x1B, table, {"SHL", 0, 2, true, false, 3, 0});
        add_opcode(0x1C, table, {"SHR", 0, 2, true, false, 3, 0});
        add_opcode(0x1D, table, {"SAR", 0, 2, true, false, 3, 0});
        add_opcode(0x3F, table, {"EXTCODEHASH", 0, 1, true, true, 100, 0});
        add_opcode(0xF5, table, {"CREATE2", 0, 4, true, true, 3200, 0});

        return table;
    }

    template <>
    consteval std::array<OpCodeInfo, 256> opcode_table<EVMC_ISTANBUL>()
    {
        auto table = opcode_table<previous_evm_revision(EVMC_ISTANBUL)>();

        add_opcode(0x46, table, {"CHAINID", 0, 0, true, false, 2, 0});
        add_opcode(0x47, table, {"SELFBALANCE", 0, 0, true, false, 5, 0});

        return table;
    }

    template <>
    consteval std::array<OpCodeInfo, 256> opcode_table<EVMC_LONDON>()
    {
        auto table = opcode_table<previous_evm_revision(EVMC_LONDON)>();

        add_opcode(0x48, table, {"BASEFEE", 0, 0, true, false, 2, 0});

        return table;
    }

    template <>
    consteval std::array<OpCodeInfo, 256> opcode_table<EVMC_PARIS>()
    {
        auto table = opcode_table<previous_evm_revision(EVMC_PARIS)>();

        table[0x44].name = "PREVRANDAO";

        return table;
    }

    template <>
    consteval std::array<OpCodeInfo, 256> opcode_table<EVMC_SHANGHAI>()
    {
        auto table = opcode_table<previous_evm_revision(EVMC_SHANGHAI)>();

        add_opcode(0x5F, table, {"PUSH0", 0, 0, true, false, 2, 0});

        return table;
    }

    template <>
    consteval std::array<OpCodeInfo, 256> opcode_table<EVMC_CANCUN>()
    {
        auto table = opcode_table<previous_evm_revision(EVMC_CANCUN)>();

        add_opcode(0x49, table, {"BLOBHASH", 0, 1, true, false, 3, 0});
        add_opcode(0x4A, table, {"BLOBBASEFEE", 0, 0, true, false, 2, 0});
        add_opcode(0x5C, table, {"TLOAD", 0, 1, true, false, 100, 0});
        add_opcode(0x5D, table, {"TSTORE", 0, 2, false, false, 100, 0});
        add_opcode(0x5E, table, {"MCOPY", 0, 3, false, true, 3, 0});

        return table;
    }

    /**
     * Mnemonic mapping of human-readable opcode names to their underlying byte
     * values.
     */
    enum EvmOpCode : uint8_t
    {
        STOP = 0x00,
        ADD = 0x01,
        MUL = 0x02,
        SUB = 0x03,
        DIV = 0x04,
        SDIV = 0x05,
        MOD = 0x06,
        SMOD = 0x07,
        ADDMOD = 0x08,
        MULMOD = 0x09,
        EXP = 0x0A,
        SIGNEXTEND = 0x0B,
        LT = 0x10,
        GT = 0x11,
        SLT = 0x12,
        SGT = 0x13,
        EQ = 0x14,
        ISZERO = 0x15,
        AND = 0x16,
        OR = 0x17,
        XOR = 0x18,
        NOT = 0x19,
        BYTE = 0x1A,
        SHL = 0x1B,
        SHR = 0x1C,
        SAR = 0x1D,
        SHA3 = 0x20,
        ADDRESS = 0x30,
        BALANCE = 0x31,
        ORIGIN = 0x32,
        CALLER = 0x33,
        CALLVALUE = 0x34,
        CALLDATALOAD = 0x35,
        CALLDATASIZE = 0x36,
        CALLDATACOPY = 0x37,
        CODESIZE = 0x38,
        CODECOPY = 0x39,
        GASPRICE = 0x3A,
        EXTCODESIZE = 0x3B,
        EXTCODECOPY = 0x3C,
        RETURNDATASIZE = 0x3D,
        RETURNDATACOPY = 0x3E,
        EXTCODEHASH = 0x3F,
        BLOCKHASH = 0x40,
        COINBASE = 0x41,
        TIMESTAMP = 0x42,
        NUMBER = 0x43,
        DIFFICULTY = 0x44,
        GASLIMIT = 0x45,
        CHAINID = 0x46,
        SELFBALANCE = 0x47,
        BASEFEE = 0x48,
        BLOBHASH = 0x49,
        BLOBBASEFEE = 0x4A,
        POP = 0x50,
        MLOAD = 0x51,
        MSTORE = 0x52,
        MSTORE8 = 0x53,
        SLOAD = 0x54,
        SSTORE = 0x55,
        JUMP = 0x56,
        JUMPI = 0x57,
        PC = 0x58,
        MSIZE = 0x59,
        GAS = 0x5A,
        JUMPDEST = 0x5B,
        TLOAD = 0x5C,
        TSTORE = 0x5D,
        MCOPY = 0x5E,
        PUSH0 = 0x5F,
        PUSH1 = 0x60,
        PUSH2 = 0x61,
        PUSH3 = 0x62,
        PUSH4 = 0x63,
        PUSH5 = 0x64,
        PUSH6 = 0x65,
        PUSH7 = 0x66,
        PUSH8 = 0x67,
        PUSH9 = 0x68,
        PUSH10 = 0x69,
        PUSH11 = 0x6A,
        PUSH12 = 0x6B,
        PUSH13 = 0x6C,
        PUSH14 = 0x6D,
        PUSH15 = 0x6E,
        PUSH16 = 0x6F,
        PUSH17 = 0x70,
        PUSH18 = 0x71,
        PUSH19 = 0x72,
        PUSH20 = 0x73,
        PUSH21 = 0x74,
        PUSH22 = 0x75,
        PUSH23 = 0x76,
        PUSH24 = 0x77,
        PUSH25 = 0x78,
        PUSH26 = 0x79,
        PUSH27 = 0x7A,
        PUSH28 = 0x7B,
        PUSH29 = 0x7C,
        PUSH30 = 0x7D,
        PUSH31 = 0x7E,
        PUSH32 = 0x7F,
        DUP1 = 0x80,
        DUP2 = 0x81,
        DUP3 = 0x82,
        DUP4 = 0x83,
        DUP5 = 0x84,
        DUP6 = 0x85,
        DUP7 = 0x86,
        DUP8 = 0x87,
        DUP9 = 0x88,
        DUP10 = 0x89,
        DUP11 = 0x8A,
        DUP12 = 0x8B,
        DUP13 = 0x8C,
        DUP14 = 0x8D,
        DUP15 = 0x8E,
        DUP16 = 0x8F,
        SWAP1 = 0x90,
        SWAP2 = 0x91,
        SWAP3 = 0x92,
        SWAP4 = 0x93,
        SWAP5 = 0x94,
        SWAP6 = 0x95,
        SWAP7 = 0x96,
        SWAP8 = 0x97,
        SWAP9 = 0x98,
        SWAP10 = 0x99,
        SWAP11 = 0x9A,
        SWAP12 = 0x9B,
        SWAP13 = 0x9C,
        SWAP14 = 0x9D,
        SWAP15 = 0x9E,
        SWAP16 = 0x9F,
        LOG0 = 0xA0,
        LOG1 = 0xA1,
        LOG2 = 0xA2,
        LOG3 = 0xA3,
        LOG4 = 0xA4,
        CREATE = 0xF0,
        CALL = 0xF1,
        CALLCODE = 0xF2,
        RETURN = 0xF3,
        DELEGATECALL = 0xF4,
        CREATE2 = 0xF5,
        STATICCALL = 0xFA,
        REVERT = 0xFD,
        SELFDESTRUCT = 0xFF
    };

    /**
     * Returns `true` if `opcode` belongs to the `PUSHN` family of EVM opcodes.
     */
    constexpr bool is_push_opcode(uint8_t const opcode)
    {
        return opcode >= PUSH0 && opcode <= PUSH32;
    }

    /**
     * Returns `true` if `opcode` belongs to the `SWAPN` family of EVM opcodes.
     */
    constexpr bool is_swap_opcode(uint8_t const opcode)
    {
        return opcode >= SWAP1 && opcode <= SWAP16;
    }

    /**
     * Returns `true` if `opcode` belongs to the `DUPN` family of EVM opcodes.
     */
    constexpr bool is_dup_opcode(uint8_t const opcode)
    {
        return opcode >= DUP1 && opcode <= DUP16;
    }

    /**
     * Returns `true` if `opcode` belongs to the `LOGN` family of EVM opcodes.
     */
    constexpr bool is_log_opcode(uint8_t const opcode)
    {
        return opcode >= LOG0 && opcode <= LOG4;
    }

    /**
     * Returns `true` if `opcode` is an invalid opcode at this revision.
     */
    template <evmc_revision Rev>
    constexpr bool is_unknown_opcode(uint8_t const opcode)
    {
        return opcode_table<Rev>()[opcode] == unknown_opcode_info;
    }

    /**
     * Returns `true` if `opcode` causes non-linear control flow when executed.
     */
    template <evmc_revision Rev>
    constexpr bool is_control_flow_opcode(uint8_t const opcode)
    {
        return opcode == STOP || opcode == JUMP || opcode == JUMPI ||
               opcode == RETURN || opcode == REVERT || opcode == SELFDESTRUCT ||
               is_unknown_opcode<Rev>(opcode);
    }

    /**
     * Opcode must be the opcode of some DUPN instruction.
     * Returns `N`.
     */
    constexpr uint8_t get_dup_opcode_index(uint8_t const opcode)
    {
        MONAD_COMPILER_DEBUG_ASSERT(is_dup_opcode(opcode));
        return opcode - DUP1 + 1;
    }

    /**
     * Opcode must be the opcode of some SWAPN instruction.
     * Returns `N`.
     */
    constexpr uint8_t get_swap_opcode_index(uint8_t const opcode)
    {
        MONAD_COMPILER_DEBUG_ASSERT(is_swap_opcode(opcode));
        return opcode - SWAP1 + 1;
    }

    /**
     * Opcode must be the opcode of some PUSHN instruction.
     * Returns `N`.
     */
    constexpr uint8_t get_push_opcode_index(uint8_t const opcode)
    {
        MONAD_COMPILER_DEBUG_ASSERT(is_push_opcode(opcode));
        return opcode - PUSH0;
    }

    /**
     * Opcode must be the opcode of some LOGN instruction.
     * Returns `N`.
     */
    constexpr uint8_t get_log_opcode_index(uint8_t const opcode)
    {
        MONAD_COMPILER_DEBUG_ASSERT(is_log_opcode(opcode));
        return opcode - LOG0;
    }

    /**
     * Opcode must be the opcode of some DUPN, SWAPN, PUSHN or LOGN instruction.
     * Returns `N`.
     */
    constexpr uint8_t get_opcode_index(uint8_t const opcode)
    {
        if (is_dup_opcode(opcode)) {
            return get_dup_opcode_index(opcode);
        }

        if (is_swap_opcode(opcode)) {
            return get_swap_opcode_index(opcode);
        }

        if (is_push_opcode(opcode)) {
            return get_push_opcode_index(opcode);
        }

        if (is_log_opcode(opcode)) {
            return get_log_opcode_index(opcode);
        }

        return 0;
    }
}
