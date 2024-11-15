#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs =  std::filesystem;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    LOG_TO_CONSOLE();
    std::optional<cmd_parser::Args> args;
    try{
        args = cmd_parser::ParseCommandLine(argc, argv);
        if(!args.has_value()){
            LOG_SERVER_EXIT(0);
            return EXIT_SUCCESS;
        }
    }catch(const std::exception& ex){
        LOG_SERVER_EXIT(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }

    try {
        const static unsigned NUM_THREADS = std::thread::hardware_concurrency();
        const char* DB_URL = std::getenv("GAME_DB_URL");
        if (!DB_URL) {
            throw std::runtime_error("GAME_DB_URL is not specified");
        }
        db_connection::CreateTable(DB_URL);
        auto db_manager = std::make_unique<db_connection::DatabaseManager>(NUM_THREADS, DB_URL);

        const cmd_parser::Args& received_args = args.value();
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(received_args.config_file);

        // 2. Инициализируем io_context
        net::io_context ioc(NUM_THREADS);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
                std::cout << std::endl;
            }
        });   

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры. 
        //    А также устанавливаем слушаетеля, который сохраняет (сериализует) состояние
        //    игры синхронно ходу игровым часам.
        std::shared_ptr<request_handler::RequestHandler> handler = std::make_shared<request_handler::RequestHandler>(game, received_args, net::make_strand(ioc), std::move(db_manager));

        // 5. Если был указан файл с сохранением игрового состояния, 
        //    то нужно попытаться восстанавливать его.
        //    Если он некорректен, то приложение завершится с ошибкой
        handler->LoadState();

        // 6. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        LOG_SERVER_START(port, address.to_string());

        // 7. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, NUM_THREADS), [&ioc] {
            ioc.run();
        });

        // 8. Сохраняем игровое состояние при выходе
        handler->SaveState();
        
    } catch (const std::exception& ex) {
        LOG_SERVER_EXIT(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }
    LOG_SERVER_EXIT(0);
}
