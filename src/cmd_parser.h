#pragma once

#include <boost/program_options.hpp>
#include <optional>
#include <vector>
#include <iostream>

namespace cmd_parser{

struct Args {
    std::optional<unsigned> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
    std::optional<std::string> state_file;
    std::optional<unsigned> save_state_period;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]);

} // namespace cmd_parser