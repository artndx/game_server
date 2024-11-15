#pragma once
#include <boost/regex.hpp>
#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include "app.h"
#include "cmd_parser.h"
#include <iostream>
#include <filesystem>
#include <variant>
#include <unordered_map>
#include <optional>

namespace request_handler {

using namespace app;
using namespace std::literals;

namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;


namespace detail{

static const std::unordered_map< std::string, std::string> 
FILES_EXTENSIONS {
        {".htm", "text/html"}, {".html", "text/html"}, {".css", "text/css"}, 
        {".txt", "text/plain"}, {".js", "text/javascript"}, {".json", "application/json"}, 
        {".xml", "application/xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, 
        {".jpe", "image/jpeg"}, {".jpeg", "image/jpeg"},  {".gif", "image/gif"},
        {".bmp", "image/bmp"}, {".ico", "image/vnd.microsoft.icon"}, {".tiff", "image/tiff"}, 
        {".tif", "image/tiff"},  {".svg", "image/svg+xml"},  {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"}
};

std::string ToLower(std::string str);

inline char FromHexToChar(char a, char b);

std::string DecodeTarget(std::string_view req_target);

std::pair<std::string, std::string> ParseArg(std::string_view arg);

std::unordered_map<std::string, std::string> ParseTargetArgs(std::string_view req_target);

std::string MakeErrorCode(std::string_view code, std::string_view message);

bool IsMatched(const std::string& str, std::string reg_expression);

}; // namespace detail

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using VariantResponse = std::variant<StringResponse, FileResponse>;

/* 
    Предварительное объявление 
    дружественных классов
    ApiHandler и FileHanlder
    для RequestHandler
*/
class ApiHandler;
class FileHandler;

/* ------------------------ BaseHandler ----------------------------------- */

class BaseHandler{
protected:
    explicit BaseHandler() = default;

    StringResponse MakeResponse(http::status status, std::string_view body,
                                    unsigned http_version, size_t content_length, 
                                    std::string content_type);

    StringResponse MakeErrorResponse(http::status status, std::string_view code, 
                                    std::string_view message, unsigned int version);
};

/* -------------------------- ApiHandler --------------------------------- */

class ApiHandler : public BaseHandler{
    friend class RequestHandler;
    
    /*
    Класс для формирования набора методов,
    который передается в функции, проверяющие 
    запросы на наличие определенных методов
    */
    class SetMethods{
    public:
        template<typename... Args>
        SetMethods(Args&&... args){
            (methods_.insert(std::forward<Args>(args)), ...);
        }

        bool IsSame(const std::string& other) const{
            return methods_.count(other);
        }

        std::string MakeSequence() const{
            std::ostringstream oss;
            bool is_first = true;
            for(const auto& method : methods_){
                if(!is_first){
                    oss << ", ";
                }
                is_first = false;
                oss << method;
            }

            return oss.str();
        }
    private:
        std::set<std::string> methods_;
    };

public:
    template<typename Request>
    StringResponse MakeApiResponse(Request&& req){
        std::string target = std::string(req.target());
        if(detail::IsMatched(target, "(/api/v1/maps)"s)){
            return MakeMapsListsResponse(req);
        } else if(detail::IsMatched(target, "(/api/v1/maps/).+"s)) {
            return MakeMapDescResponse(req);
        } else if(detail::IsMatched(target, "(/api/v1/game/).*"s)){
            if(detail::IsMatched(target, "(/api/v1/game/join)"s)){
                return MakeAuthResponse(req);
            } else if(detail::IsMatched(target, "(/api/v1/game/players)"s)) {
                return MakePlayerListResponse(req);
            } else if(detail::IsMatched(target, "(/api/v1/game/state)"s)) {
                return MakeGameStateResponse(req);
            } else if(detail::IsMatched(target, "(/api/v1/game/tick)"s)){
                return MakeIncreaseTimeResponse(req);
            } else if(detail::IsMatched(target, "(/api/v1/game/player/action)"s)){
                return MakeActionResponse(req);
            } else if(detail::IsMatched(target, "(/api/v1/game/records).*"s)){
                return MakeRecordsResponse(req);
            }
        }
        auto res = MakeErrorResponse(http::status::bad_request, "badRequest"sv, "Bad request"sv, req.version());
        return res;
    }

    void SaveState(){
        app_.SaveState();
    }

    void LoadState(){
        app_.LoadState();
    }

private:
    explicit ApiHandler(model::Game& game, Strand api_strand, 
                        std::optional<unsigned> tick_period, 
                        std::optional<std::string> state_file, 
                        std::optional<unsigned> save_state_period, 
                        bool randomize_spawn_points,
                        DatabaseManagerPtr&& db_manager)
        : app_(game, api_strand, tick_period, state_file, save_state_period, randomize_spawn_points, std::move(db_manager)){}

    Strand& GetStrand(){
        return app_.GetStrand();
    }

    template<typename Request>
    void DumpRequest(const Request& req){
        std::cout << "HTTP/1.1 "
        << req.method_string() << ' ' 
        << req.target() << std::endl;

        // Выводим заголовки запроса
        for (const auto& header : req) {
            std::cout << "  "sv << header.name_string() << ": "sv << header.value() << std::endl;
        }

        std::cout << " "sv << req.body() << std::endl;
    }

    template<typename Response>
    void DumpResponse(const Response& res){
        std::cout << "HTTP/1.1 "
        << res.result() << std::endl;

        // Выводим заголовки ответа
        for (const auto& header : res) {
            std::cout << "  "sv << header.name_string() << ": "sv << header.value() << std::endl;
        }   

        std::cout << " "sv << res.body() << std::endl;
    }

    template<typename Request>
    StringResponse MakeMapsListsResponse(Request&& req){
        using namespace std::literals;

        SetMethods methods("GET", "HEAD");
        std::string method = std::string(req.method_string());
        if(methods.IsSame(method)){
            std::string body = app_.GetMapsList();
            return MakeResponse(http::status::ok, body, 
                                        req.version(), body.size(), "application/json"s);
        } else{
            auto res =  MakeErrorResponse(http::status::method_not_allowed, 
                "invalidMethod"sv, "Only GET method is expected"sv, req.version());
            res.insert("Allow"s, methods.MakeSequence());
            return res;
        }

        std::string body = app_.GetMapsList();
        return MakeResponse(http::status::ok, body, 
                                        req.version(), body.size(), "application/json"s);
    }

    template<typename Request>
    StringResponse MakeMapDescResponse(Request&& req){
        using namespace std::literals;

        SetMethods methods("GET", "HEAD");
        std::string method = std::string(req.method_string());
        if(methods.IsSame(method)){
            std::string req_target = std::string(req.target());
            model::Map::Id id(std::string(req_target.substr(13, req_target.npos)));
            if(auto map = app_.FindMap(id); map){
                std::string body = app_.GetMapDescription(map);
                return MakeResponse(http::status::ok, body, 
                                        req.version(), body.size(), "application/json"s);
            }

            return MakeErrorResponse(http::status::not_found, 
                "mapNotFound"sv, "Map not found"sv, req.version());
        } else {
            auto res =  MakeErrorResponse(http::status::method_not_allowed, 
                "invalidMethod"sv, "Only GET method is expected"sv, req.version());
            res.insert("Allow"s, methods.MakeSequence());
            return res;
        }
    }

    template<typename Request>
    StringResponse MakeAuthResponse(Request&& req){
        SetMethods methods("POST");
        std::string method = std::string(req.method_string());
        if(methods.IsSame(method)){
            if(req.at(http::field::content_type) == "application/json"sv){
                json::object body;
                try{
                    body = json::parse(req.body()).as_object();
                } catch(std::exception& ex){
                    return MakeErrorResponse(http::status::bad_request, 
                        "invalidArgument"sv, "Join game request parse error"sv, req.version());
                }
                
                if(body.count("userName"s) && body.count("mapId")){
                    std::string user_name = std::string(body.at("userName"s).as_string());
                    std::string map_id = std::string(body.at("mapId"s).as_string());

                    if(user_name.empty()){
                        return MakeErrorResponse(http::status::bad_request, 
                            "invalidArgument"sv, "Invalid name"sv, req.version());
                    }

                    if(!app_.FindMap(model::Map::Id(map_id))){
                        return MakeErrorResponse(http::status::not_found, 
                            "mapNotFound"sv, "Map not found"sv, req.version());
                    }
                    /* Запрос без ошибок */
                    std::string body = app_.GetJoinGameResult(user_name, map_id);
                    return MakeResponse(http::status::ok, body, req.version(), body.size(), 
                        "application/json"s);
                }
                return MakeErrorResponse(http::status::bad_request, 
                    "invalidArgument"sv, "userName and mapId is expected"sv, req.version());
            }
            return MakeErrorResponse(http::status::bad_request, 
                "invalidArgument"sv, "Content-Type: application/json expected"sv, req.version());
        }
        auto res =  MakeErrorResponse(http::status::method_not_allowed, 
            "invalidMethod"sv, "Only POST method is expected"sv, req.version());
        res.insert("Allow"s, methods.MakeSequence());
        return res;
    }
    /* 
        Проверяет на правильность запрос 
        с авторизационным токеном.
        Если запрос невалиден, возвращает ответ с кодом ошибки.
        Если валиден, то вызывает функцию action 
        с переданным ей запросом.
    */
    template <typename Request, typename Fn>
    StringResponse ExecuteAuthorized(const SetMethods& methods, Request&& req, Fn&& action) {
        std::string method = std::string(req.method_string());
        if(methods.IsSame(method)){
            auto it = req.find(http::field::authorization);
            try{
                if(it != req.end()){
                    std::string_view req_token = it->value();
                    Token token(std::string(req_token.substr(7, req_token.npos)));
                    if((*token).size() != 32){
                        throw std::logic_error("Incorrect token");
                    }

                    if(app_.FindPlayerByToken(token)){
                        /* Запрос без ошибок */
                        return action(std::move(req), token);
                    }

                    return MakeErrorResponse(http::status::unauthorized, 
                        "unknownToken"sv, "Player token has not been found"sv, req.version());
                } else {
                    throw std::logic_error("Token is missing");
                }
            } catch(...){
                return MakeErrorResponse(http::status::unauthorized, 
                    "invalidToken"sv, "Authorization header is missing"sv, req.version());
            }
        }

        auto res =  MakeErrorResponse(http::status::method_not_allowed, 
            "invalidMethod"sv, "Invalid method"sv, req.version());
        res.insert("Allow"s, methods.MakeSequence());
        return res;
    }

    template<typename Request>
    StringResponse MakePlayerListResponse(Request&& req){
        SetMethods available_methods("GET", "HEAD");
        return ExecuteAuthorized(available_methods, req, [this](Request&& req, const Token& token){
                std::string body = this->app_.GetPlayerList(token);
                return this->MakeResponse(http::status::ok, body, req.version(), body.size(), 
                    "application/json"s);
        });
    }

    template<typename Request>
    StringResponse MakeGameStateResponse(Request&& req){
        SetMethods available_methods("GET", "HEAD");
        return ExecuteAuthorized(available_methods, req, [this](Request&& req, const Token& token){
                std::string body = this->app_.GetGameState(token);
                return this->MakeResponse(http::status::ok, body, req.version(), body.size(), 
                    "application/json"s);
        });
    }

    template<typename Request>
    StringResponse MakeIncreaseTimeResponse(Request&& req){
        if(app_.IsPeriodicMode()){
            return MakeErrorResponse(http::status::bad_request, "badRequest"sv, "Invalid endpoint"sv, req.version());
        }
        if(req.method_string() == "POST"){
            auto it = req.find(http::field::content_type);
            if(it != req.end()){
                if(it->value() == "application/json"s){
                    json::object body;
                    try{
                        body = json::parse(req.body()).as_object();
                        unsigned delta = static_cast<double>(body.at("timeDelta"s).as_int64());

                        /* Запрос без ошибок */
                        std::string body = app_.IncreaseTime(delta);
                        return MakeResponse(http::status::ok, body, req.version(), body.size(), 
                            "application/json"s);
                    } catch(std::exception& ex){
                        return MakeErrorResponse(http::status::bad_request, 
                            "invalidArgument"sv, "Failed to parse tick request JSON"sv, req.version());
                    }
                }
            } 

            return MakeErrorResponse(http::status::bad_request, 
            "invalidArgument"sv, "Invalid content type - application/json is required"sv, req.version());
        }

        return MakeErrorResponse(http::status::method_not_allowed, 
            "invalidMethod"sv, "Invalid method"sv, req.version());
    }

    template<typename Request>
    StringResponse MakeActionResponse(Request&& req){
        if(auto it = req.find(http::field::content_type); it != req.end()){
            if(it->value() == "application/json"s){
                try{
                    json::object action = json::parse(req.body()).as_object();
                    if(auto it = action.find("move"); it != action.end()){
                        /* Запрос без ошибок */
                        SetMethods available_methods("POST");
                        return ExecuteAuthorized(available_methods, req, [this, &action](Request&& req, const Token& token){
                            std::string body = this->app_.ApplyPlayerAction(action, token);
                            return this->MakeResponse(http::status::ok, body, req.version(), body.size(), 
                            "application/json"s);
                        });
                    } else{
                        throw std::runtime_error("Failed to parse action");
                    }

                } catch(std::exception& ex){
                    auto res =  MakeErrorResponse(http::status::bad_request, 
                    "invalidArgument"sv, "Failed to parse action"sv, req.version());
                    return res;
                }
            }
        }
        auto res =  MakeErrorResponse(http::status::bad_request, 
        "invalidArgument"sv, "Invalid content type"sv, req.version());
        return res;
    }

    template<typename Request>
    StringResponse MakeRecordsResponse(Request&& req){
        SetMethods methods("GET", "HEAD");
        std::string method = std::string(req.method_string());
        if(methods.IsSame(method)){
            std::string target = std::string(req.target());
            
            unsigned start = 0;
            unsigned max_items = 100;
            try{
                auto url_args = detail::ParseTargetArgs(target);
                if(url_args.contains("start")){
                    start = std::stol(url_args.at("start"));
                }

                if(url_args.contains("maxItems")){
                    max_items = std::stol(url_args.at("maxItems"));
                }
            } catch(...){ 
            }

            if(max_items > 100){
                throw std::logic_error("Incorrect maxItems parameter");
            }
            std::string body = app_.GetRecords(start, max_items);
            return MakeResponse(http::status::ok, body, 
                                        req.version(), body.size(), "application/json"s);
        }

        auto res =  MakeErrorResponse(http::status::method_not_allowed, 
            "invalidMethod"sv, "Only GET method is expected"sv, req.version());
        res.insert("Allow"s, methods.MakeSequence());
        return res;
        
    }   

    Application app_;
};

/* -------------------------- FileHandler --------------------------------- */

class FileHandler : public BaseHandler{
    friend class RequestHandler;
public:
    template<typename Request>
    VariantResponse MakeFileResponse(Request&& req){    
        FileResponse response;
        if(req.target() == "/"){
            req.target("/index.html");
        }

        http::file_body::value_type file;
        std::string uncoded_target = detail::DecodeTarget(req.target().substr(1));
        std::string content_type = GetRequiredContentType(req.target());

        fs::path required_path(uncoded_target);
        fs::path summary_path = fs::weakly_canonical(static_path_ / required_path);
        if (sys::error_code ec; file.open(summary_path.string().data(), beast::file_mode::read, ec), ec) {
            std::string empty_body;
            return MakeResponse(http::status::not_found, empty_body,  
                                            req.version(), empty_body.size(), "text/plain");
        }
        
        response.version(req.version());
        response.result(http::status::ok);
        response.insert(http::field::content_type, content_type);
        response.body() = std::move(file);
        // Метод prepare_payload заполняет заголовки Content-Length и Transfer-Encoding
        // в зависимости от свойств тела сообщения
        response.prepare_payload();

        return response;
    }
private:
    explicit FileHandler(fs::path static_path)
        : static_path_(fs::canonical(static_path)){
    }

    std::string GetRequiredContentType(std::string_view req_target);

    fs::path static_path_;
};

/* ------------------------- RequestHandler ---------------------------------- */

class RequestHandler : public std::enable_shared_from_this<RequestHandler>{
public:
    explicit RequestHandler(model::Game& game, const cmd_parser::Args& args, Strand api_strand, DatabaseManagerPtr&& db_manager)
        : game_{game}, 
        api_handler_{game, api_strand, args.tick_period, args.state_file, args.save_state_period, args.randomize_spawn_points, std::move(db_manager)},
        file_handler_{args.www_root}{}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template<typename Request, typename Send>
    void operator()(Request&& req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
    
        /* Api запросы обрабатывает ApiHandler*/
        if(detail::IsMatched(std::string(req.target()), "(/api/).*")){
            auto handle = [self = shared_from_this(), send, req] {
                try {
                    // Этот assert не выстрелит, так как лямбда-функция будет выполняться внутри strand
                    assert(self->api_handler_.GetStrand().running_in_this_thread());
                    return send(self->api_handler_.MakeApiResponse(req));
                } catch (...) {
                    send(self->api_handler_.MakeErrorResponse(http::status::bad_request, 
                        "badRequest"sv, "Bad request"sv, req.version()));
                }
            };
            return net::dispatch(api_handler_.GetStrand(), handle);
        }

        /* Запросы доступа к файлам обрабатывает FileHandler*/
        return std::visit(
                [&send, &req](auto&& result) {
                    send(std::forward<decltype(result)>(result));
                },
                file_handler_.MakeFileResponse(std::forward<decltype(req)>(req)));  
    }

    void SaveState(){
        api_handler_.SaveState();
    }

    void LoadState(){
        api_handler_.LoadState();
    }

private:
    model::Game& game_;
    ApiHandler api_handler_;
    FileHandler file_handler_;
};

}  // namespace request_handler
