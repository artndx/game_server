#pragma once
#include <boost/log/trivial.hpp>     
#include <boost/log/core.hpp>        
#include <boost/log/expressions.hpp> 
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/json.hpp>
#include <unordered_map>
#include <chrono>

/* Запуск сервера */
#define LOG_SERVER_START(port, address) \
    logger::Log({{"port"s, port}, {"address"s, address}}, logger::LOG_MESSAGES::SERVER_STARTED);  

/* Остановка сервера */
#define LOG_SERVER_EXIT(code, ...) \
        logger::Log({{"code"s, code} __VA_OPT__(, {"exception"s, __VA_ARGS__})}, logger::LOG_MESSAGES::SERVER_EXITED); 

/* Получение запроса */
#define LOG_REQUEST_RECEIVED(ip, URL, method) \
    logger::Log({{"ip"s, ip}, {"URL"s, URL}, {"method", method}}, logger::LOG_MESSAGES::REQUEST_RECEIVED);

/* Формирование ответа */
#define LOG_RESPONSE_SENT(ip, response_time, code, content_type) \
    logger::Log({{"ip"s, ip}, {"response_time"s, response_time}, {"code"s, code}, {"content_type", content_type}}, logger::LOG_MESSAGES::RESPONSE_SENT);

/* Возникновение ошибки */
#define LOG_ERROR(code, text, where) \
    logger::Log({{"code"s, code}, {"text"s, text}, {"where", where}}, logger::LOG_MESSAGES::ERROR);


namespace logger{

namespace logging = boost::log;
namespace keywords = logging::keywords;
namespace json = boost::json;

enum class LOG_MESSAGES{
    SERVER_STARTED,
    SERVER_EXITED,
    REQUEST_RECEIVED,
    RESPONSE_SENT,
    ERROR
};

class Timer{
public:
    void Start(){
        start_ = std::chrono::system_clock::now();
    }

    size_t End(){
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_).count();
        start_.min();
        return dur;
    }
private:
    std::chrono::system_clock::time_point start_ = std::chrono::system_clock::now();
};

static const std::unordered_map<LOG_MESSAGES, std::string> STR_MESSAGES {
    {LOG_MESSAGES::SERVER_STARTED, "Server has started..."},
    {LOG_MESSAGES::SERVER_EXITED, "server exited"},
    {LOG_MESSAGES::REQUEST_RECEIVED, "request received"},
    {LOG_MESSAGES::RESPONSE_SENT, "response sent"},
    {LOG_MESSAGES::ERROR, "error"},
};

std::ostream& operator<<(std::ostream& out, LOG_MESSAGES msg);

void JSONFormatter(logging::record_view const& rec, logging::formatting_ostream& strm);

void FileConfig();

void ConsoleConfig();

void Log(const json::value& data, LOG_MESSAGES message);

}; // namespace logger

/* Настройка для вывода в консоль */
#define LOG_TO_CONSOLE() logger::ConsoleConfig();

/* Настройка для вывода в файл */
#define LOG_TO_FILE() logger::FileConfig();