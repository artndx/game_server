#include "connection_pool.h"

namespace db_connection{

/* ------------------------ ConnectionPool ----------------------------------- */

ConnectionPool::ConnectionWrapper ConnectionPool::GetConnection(){
    std::unique_lock lock{mutex_};
    // Блокируем текущий поток и ждём, пока cond_var_ не получит уведомление и не освободится
    // хотя бы одно соединение
    cond_var_.wait(lock, [this] {
        return used_connections_ < pool_.size();
    });
    // После выхода из цикла ожидания мьютекс остаётся захваченным

    return {std::move(pool_[used_connections_++]), *this};
}


void ConnectionPool::ReturnConnection(ConnectionPool::ConnectionPtr&& conn){
    // Возвращаем соединение обратно в пул
    {
        std::lock_guard lock{mutex_};
        assert(used_connections_ != 0);
        pool_[--used_connections_] = std::move(conn);
    }
    // Уведомляем один из ожидающих потоков об изменении состояния пула
    cond_var_.notify_one();
}

/* ------------------------ ConnectionFactory ----------------------------------- */

inline ConnectionPool::ConnectionPtr ConnectionFactory(const char* db_url){
    auto conn = std::make_shared<pqxx::connection>(db_url);
    conn->prepare("insert", R"(
                        INSERT INTO retired_players (name, score, time) VALUES ($1, $2, $3);
                        )");
    conn->prepare("select", R"(
                        SELECT name, score, time FROM retired_players 
                        ORDER BY score DESC, time, name 
                        LIMIT $1 
                        OFFSET $2;
                        )");
    return conn;
}

/* ------------------------ DatabaseManager ----------------------------------- */

DatabaseManager::DatabaseManager(size_t capacity, const char* db_url)
:connection_pool_(capacity, ConnectionFactory, db_url){}

pqxx::result DatabaseManager::SelectData(unsigned start, unsigned max_items){
    auto conn = connection_pool_.GetConnection();
    pqxx::read_transaction rt{*conn};
    return rt.exec_prepared("select", max_items, start);
}

void DatabaseManager::InsertData(std::string_view name, unsigned score, double time){
    auto conn = connection_pool_.GetConnection();
    pqxx::work w{*conn};
    w.exec_prepared("insert", name, score , time);
    w.commit();
}

/* ------------------------ CreateTable ----------------------------------- */

void CreateTable(const char* db_url){
    using pqxx::operator""_zv;
    using namespace std::literals;

    auto initial_connection_factory = [](const char* db_url){
        auto conn = std::make_shared<pqxx::connection>(db_url);
        return conn;
    };

    db_connection::ConnectionPool connection_pool_(1, initial_connection_factory, db_url);
    auto conn = connection_pool_.GetConnection();
    pqxx::work work{*conn};

    work.exec(R"(
        DROP TABLE IF EXISTS retired_players;
        )"_zv);

    work.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id SERIAL PRIMARY KEY,
            name varchar(100) NOT NULL,
            score integer NOT NULL,
            time real NOT NULL
        );
        )"_zv);
    work.exec(R"(
            CREATE INDEX IF NOT EXISTS score_time_name_idx ON retired_players (score DESC, time, name);
    )");
    work.commit();
}

} // namespace db_connection