#include "logger.h"
#include <filesystem>
#include <sstream>

using namespace std::literals;

namespace logger{

std::ostream& operator<<(std::ostream& out, LOG_MESSAGES msg){
    out << STR_MESSAGES.at(msg);
    return out;
}

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void JSONFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object log;

    log.emplace("timestamp", to_iso_extended_string(*rec[timestamp]));
    log.emplace("data", *rec[additional_data]);
    log.emplace("message", *rec[logging::expressions::smessage]);

    strm << json::serialize(log);
}

void FileConfig(){
    logging::add_common_attributes();
    std::filesystem::path file_path = std::filesystem::weakly_canonical( std::filesystem::path("../../logs/") / std::filesystem::path ("sample_%N.json"));
    logging::add_file_log(
        keywords::file_name = file_path.string(),
        keywords::format = &JSONFormatter,
        keywords::open_mode = std::ios_base::app | std::ios_base::out,
        // ротируем по достижению размера 10 мегабайт
        keywords::rotation_size = 10 * 1024 * 1024,
        // ротируем ежедневно в полдень
        keywords::time_based_rotation = logging::sinks::file::rotation_at_time_point(12, 0, 0)
    );
}

void ConsoleConfig(){
    logging::add_common_attributes();
    logging::add_console_log( 
        std::clog,
        keywords::format = &JSONFormatter,
        keywords::auto_flush = true
    );
}

void Log(const json::value& data, LOG_MESSAGES message){
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                            << message;
}

}; // namespace logger