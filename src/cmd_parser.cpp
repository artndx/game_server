#include "cmd_parser.h"

namespace cmd_parser{

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    using namespace std;
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"s};

    Args args;
    unsigned tick_period;
    std::string state_file;
    unsigned save_state_period;
;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&tick_period)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("config-file"s), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions ")
        ("state-file", po::value(&state_file)->value_name("state-file"s), "set file path, which saves a game state in procces, and restore it at startup")
        ("save-state-period", po::value(&save_state_period)->value_name("milliseconds"s), "set period for automatic saving of game state.");
        
    // variables_map хранит значения опций после разбора
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        // Если был указан параметр --help, то выводим справку и возвращаем nullopt
        std::cout << desc;
        return std::nullopt;
    }

    if (vm.contains("tick-period"s)) {
        args.tick_period = tick_period;
    }

    if (vm.contains("state-file"s)) {
        args.state_file = state_file;
    }
    
    if (vm.contains("save-state-period"s)) {
        args.save_state_period = save_state_period;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file path is not specified : Usage game_server -c <file> --w <dir>"s);
    }

    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Static files path is not specified : Usage game_server -c <file> --w <dir>"s);
    }

    if (vm.contains("randomize-spawn-points"s)) {
        args.randomize_spawn_points = true;
    }

    // С опциями программы всё в порядке, возвращаем структуру args
    return args;
}

} // namespace cmd_parser