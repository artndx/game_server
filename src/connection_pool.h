# pragma once
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace db_connection{

using pqxx::operator""_zv;
using namespace std::literals;

/* ------------------------ ConnectionPool ----------------------------------- */

class ConnectionPool {
public:
    using PoolType = ConnectionPool;
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;
    class ConnectionWrapper {
    public:
        ConnectionWrapper(std::shared_ptr<pqxx::connection>&& conn, PoolType& pool) noexcept
            : conn_{std::move(conn)}
            , pool_{&pool} {
        }

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

        pqxx::connection& operator*() const& noexcept {
            return *conn_;
        }
        pqxx::connection& operator*() const&& = delete;

        pqxx::connection* operator->() const& noexcept {
            return conn_.get();
        }

        ~ConnectionWrapper() {
            if (conn_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }

    private:
        std::shared_ptr<pqxx::connection> conn_;
        PoolType* pool_;
    };

    // ConnectionFactory is a functional object returning std::shared_ptr<pqxx::connection>
    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory, const char* db_url){
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory(db_url));
        }
    }

    ConnectionWrapper GetConnection();

private:
    void ReturnConnection(ConnectionPtr&& conn);

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_connections_ = 0;
};

/* ------------------------ ConnectionFactory ----------------------------------- */

ConnectionPool::ConnectionPtr ConnectionFactory(const char* db_url);

/* ------------------------ DatabaseManager ----------------------------------- */

class DatabaseManager{
public:
    DatabaseManager(size_t capacity, const char* db_url);

    pqxx::result SelectData(unsigned start, unsigned max_items);

    void InsertData(std::string_view name, unsigned score, double time);

private:
    ConnectionPool connection_pool_;
};

/* ------------------------ CreateTable ----------------------------------- */

void CreateTable(const char* db_url);

} // namespace db_connection