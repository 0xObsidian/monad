#include "cli_tool_impl.hpp"

#include <iostream>
#include <vector>

int main(int argc_, char *argv[])
{
    size_t const argc = size_t(argc_);
    std::vector<std::string_view> args(argc);
    for (size_t n = 0; n < argc; n++) {
        args[n] = argv[n];
    }
    return main_impl(std::cout, std::cerr, args);
}
