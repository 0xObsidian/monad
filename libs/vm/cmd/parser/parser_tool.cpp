/**
 * parse an evm opcode text <file> and
 * write the resulting evm bytecode to <file>.evm
 *
 * -b switch reads in an evm bytecode file and
 *  writes the corresponding text to stdout
 *
 * see parser.hpp for details
 *
 */

#include <cstdint>
#include <cstdlib>
#include <format>
#include <ios>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <evmc/evmc.h>

#include <monad/vm/compiler/ir/x86.hpp>
#include <monad/vm/utils/parser.hpp>

using namespace monad::vm::utils;

struct arguments
{
    bool verbose = false;
    bool binary = false;
    bool stdin = false;
    bool compile = false;
    bool validate = false;
    std::vector<std::string> filenames;
};

arguments parse_args(int const argc, char **const argv)
{
    auto app = CLI::App("evm opcode parser/printer");
    auto args = arguments{};

    app.add_option("filenames", args.filenames, "List of files to process")
        ->check(CLI::ExistingFile);

    app.add_flag(
        "-b,--binary",
        args.binary,
        "process input files as binary and show evm opcodes/data as text");

    app.add_flag("-c,--compile", args.compile, "compile the input files");
    app.add_option(
        "--validate",
        args.validate,
        std::format(
            "validate input files (default: {})"
            " - reports incorrect errors in some programs",
            args.validate));

    app.add_flag("-v,--verbose", args.verbose, "send debug info to stdout");

    app.add_flag(
        "-s,--stdin", args.stdin, "read from stdin and write to stdout");

    try {
        app.parse(argc, argv);
    }
    catch (CLI::ParseError const &e) {
        std::exit(app.exit(e));
    }

    return args;
}

std::vector<uint8_t> do_parse(
    parser_config const &config, std::string const &filename,
    std::string const &s, std::string const &outfile, std::ostream &os)
{
    if (config.verbose) {
        std::cerr << "parsing " << filename << '\n';
    }

    auto opcodes = parse_opcodes(config, s);
    if (config.verbose) {
        std::cerr << "writing " << outfile << '\n';
    }
    os.write(
        reinterpret_cast<char const *>(opcodes.data()),
        static_cast<long>(opcodes.size()));
    return opcodes;
}

void do_binary(
    parser_config const &config, std::string const &filename,
    std::vector<uint8_t> const &opcodes)
{
    if (config.verbose) {
        std::cerr << "printing " << filename << '\n';
    }

    std::cout << show_opcodes(opcodes) << '\n';
}

int main(int argc, char **argv)
{
    auto args = parse_args(argc, argv);

    parser_config const config{args.verbose, args.validate};

    if (args.stdin) {
        std::stringstream buffer;
        buffer << std::cin.rdbuf();
        std::string s = buffer.str();
        auto opcodes =
            args.binary ? std::vector<uint8_t>(s.begin(), s.end())
                        : do_parse(config, "<stdin>", s, "<stdout>", std::cout);

        if (args.binary) {
            do_binary(config, "<stdin>", opcodes);
        }

        if (args.compile) {
            auto rt = asmjit::JitRuntime{};
            monad::vm::compiler::native::compile(
                rt,
                opcodes,
                EVMC_LATEST_STABLE_REVISION,
                {.asm_log_path = "out.asm"});
        }
    }

    for (auto const &filename : args.filenames) {
        auto in = std::ifstream(filename, std::ios::binary);
        if (args.binary) {
            auto opcodes = std::vector<uint8_t>(
                std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>());
            do_binary(config, filename, opcodes);
        }
        else {
            auto s = std::string(
                std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>());
            auto outfile = filename + ".evm";
            auto os = std::ofstream(outfile, std::ios::binary);
            auto opcodes = do_parse(config, filename, s, outfile, os);
            if (args.compile) {
                auto outfile_asm = filename + ".asm";
                auto rt = asmjit::JitRuntime{};
                monad::vm::compiler::native::compile(
                    rt,
                    opcodes,
                    EVMC_LATEST_STABLE_REVISION,
                    {.asm_log_path = outfile_asm.c_str()});
            }
        }
    }
    return 0;
}
