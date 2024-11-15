#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json.hpp>
#include <pqxx/pqxx>
#include <chrono>
#include <sstream>
#include <optional>
#include <functional>
#include <fstream>
#include "player.h"
#include "model_serialization.h"
#include "connection_pool.h"

namespace app{

namespace json = boost::json;
namespace net = boost::asio;
namespace sys = boost::system;
using Strand = net::strand<net::io_context::executor_type>;
using Clock = std::chrono::steady_clock;
using namespace model::detail;
using namespace model;
using DatabaseManagerPtr = std::unique_ptr<db_connection::DatabaseManager>;

namespace detail{

/* ------------------------ Ticker ----------------------------------- */

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Handler = std::function<void(Milliseconds delta)>;
    
    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand& strand, Milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start();

private:
    void ScheduleTick();

    void OnTick(sys::error_code ec);


    Strand& strand_;
    Milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    Clock::time_point last_tick_;
};

/* ------------------------ PlayerTimeClock ----------------------------------- */

/* Класс для отслеживания за бездействием игрока и его игровым временем*/
class PlayerTimeClock{
public:
    PlayerTimeClock() = default;

    void IncreaseTime(size_t delta);

    std::optional<Milliseconds> GetInactivityTime() const;

    void UpdateActivity(Dog::Speed new_speed);

    Milliseconds GetPlaytime() const;
private:
    Clock::time_point log_in_time_ = Clock::now();
    Clock::time_point current_playtime = Clock::now();
    std::optional<Clock::time_point> inactivity_start_time_ = Clock::now();
    std::optional<Clock::time_point> current_inactivity_time_;
};

} // namespace detail

/* ------------------------ Use Cases ----------------------------------- */

/* ------------------------ GetMapUseCase ----------------------------------- */


class GetMapUseCase{
public:
    static std::string MakeMapDescription(const Map* map);
private:
    static json::array GetRoadsInJSON(const Map::Roads& roads);
    static json::array GetBuildingsInJSON(const Map::Buildings& buildings);
    static json::array GetOfficesInJSON(const Map::Offices& offices);
    static json::array GetLootTypesInJSON(const Map::LootTypes& loot_types);
};

/* ------------------------ ListMapsUseCase ----------------------------------- */

class ListMapsUseCase{
public:
    static std::string MakeMapsList(const Game::Maps& maps);
};

/* ------------------------ GameUseCase ----------------------------------- */

class GameUseCase{
public:
    using PlayerTimeClocks = std::unordered_map<const Player*, detail::PlayerTimeClock>;
    
    GameUseCase(Players& players, PlayerTokens& tokens, DatabaseManagerPtr&& db_manager)
        : players_(players), tokens_(tokens), db_manager_(std::move(db_manager)){}

    std::string JoinGame(const std::string& user_name, const std::string& str_map_id, 
                            Game& game, bool is_random_spawn_enabled);

    std::string GetGameState(const Token& token) const;

    std::string SetAction(const json::object& action, const Token& token);

    std::string IncreaseTime(unsigned delta, Game& game);

    static void GenerateLoot(Milliseconds delta, Game& game);

    std::string GetRecords(unsigned start, unsigned max_items);
private:
    static json::array GetBagItems(const Dog::Bag& bag_items);
    json::object GetPlayers(const PlayerTokens::PlayersInSession& players_in_session) const;
    static json::object GetLostObjects(const std::list<Loot>& loots);
    void AddPlayerTimeClock(Player* player);
    void SaveScore(const Player* player, Game& game);
    void DisconnectPlayer(const Player* player, Game& game);

    int auto_counter_ = 0;
    Players& players_;
    PlayerTokens& tokens_;
    PlayerTimeClocks clocks_;
    DatabaseManagerPtr db_manager_;
};

/* ------------------------ ListPlayersUseCase ----------------------------------- */

class ListPlayersUseCase{
public:
    static std::string GetPlayersInJSON(const PlayerTokens::PlayersInSession& players);
};

/* ------------------------ GameStateSaveCase ----------------------------------- */

class GameStateSaveCase{
public:
    GameStateSaveCase(std::string state_file, 
                        std::optional<unsigned> period, 
                        const Game::SessionsByMapId& sessions, 
                        const Players& players)
    : state_file_(state_file), 
    save_state_period_(period),
    sessions_(sessions),
    players_(players),
    last_tick_(Clock::now()){}

    void SaveOnTick(bool is_periodic);

    void SaveState();

    serialization::GameStateRepr LoadState();

private:
    Clock::time_point last_tick_;
    std::string state_file_;
    std::optional<unsigned> save_state_period_; 
    const Game::SessionsByMapId& sessions_;
    const Players& players_;
};

/* --------------------------- Application -------------------------------- */

class Application{
public:

    Application(Game& game, 
                Strand api_strand, 
                std::optional<unsigned> tick_period, 
                std::optional<std::string> state_file, 
                std::optional<unsigned> save_state_period,
                bool randomize_spawn_points,
                DatabaseManagerPtr&& db_manager)
        : 
        game_(game), 
        api_strand_(api_strand),
        tick_period_(tick_period), 
        rand_spawn_(randomize_spawn_points), players_(), tokens_(), 
        game_handler_(players_, tokens_, std::move(db_manager)), time_ticker_(), loot_ticker_(){
            /* Перед началом работы приложения всегда генерируется начальный лут*/
            GenerateLoot(Milliseconds{0});

            /* 
                Если в аргументах командной строки 
                указан период обновления игрового состояния,
                то создаются таймер на обновление игрового состояния 
                и таймер на обновления лута
            */
            if(tick_period_.has_value()){
                time_ticker_ = std::make_shared<detail::Ticker>(api_strand_, FromInt(*tick_period_), [this](Milliseconds delta){
                    this->IncreaseTime(delta.count() / 1000);
                });

                time_ticker_->Start();

                loot_ticker_ = std::make_shared<detail::Ticker>(api_strand_, game_.GetLootGeneratePeriod(), [this](Milliseconds delta){
                    this->GenerateLoot(delta);
                });

                loot_ticker_->Start();
            }

            if(state_file.has_value()){
                state_save_.emplace(state_file.value(), save_state_period, game_.GetAllSessions(), players_);
            }
        }
    Strand& GetStrand(){
        return api_strand_;
    }

    std::string GetMapsList() const{
        return ListMapsUseCase::MakeMapsList(game_.GetMaps());
    }

    const Map* FindMap(const Map::Id& map_id) const{
        return game_.FindMap(map_id);
    }

    const Player* FindPlayerByToken(const Token& token) const{
        return tokens_.FindPlayerByToken(token);
    }

    bool IsPeriodicMode() const{
        return tick_period_.has_value();
    }

    std::string GetMapDescription(const Map* map) const{
        return GetMapUseCase::MakeMapDescription(map);
    }

    std::string GetJoinGameResult(const std::string& user_name, const std::string& map_id){
        return game_handler_.JoinGame(user_name, map_id, game_, rand_spawn_);
    }

    std::string GetPlayerList(const Token& token) const{
        const GameSession* session = tokens_.FindPlayerByToken(token)->GetSession();
        PlayerTokens::PlayersInSession players = tokens_.GetPlayersBySession(session);
        return ListPlayersUseCase::GetPlayersInJSON(players);
    }

    std::string GetGameState(const Token& token) const{
        return game_handler_.GetGameState(token);
    }

    void SaveState(){
        if(state_save_.has_value()){
            state_save_.value().SaveState();
        }
    }

    void LoadState(){
        if(state_save_.has_value()){
            auto game_state = state_save_.value().LoadState();
            for(const auto& [map_id, sessions] : game_state.GetAllSessions()){
                for(const auto& session_repr : sessions){
                    GameSession* session =  game_.AddSession(Map::Id(map_id));
                    /* Заполнение потерянных объектов */
                    session->SetLootObjects(session_repr.GetLoot());
                    for(const auto& dog_repr : session_repr.GetDogsRepr()){
                        /* Добавление собаки */
                        Dog* created_dog = session->AddCreatedDog(dog_repr.Restore());
                        const auto& player_repr = dog_repr.GetPlayerRepr();
                        /* Добавление игрока */
                        Player& added_player = players_.Add(player_repr.GetId(), 
                                    Player::Name(player_repr.GetName()),
                                    created_dog,
                                    session);
                        tokens_.AddPlayerWithToken(added_player, Token(player_repr.GetToken()));
                        tokens_.AddPlayerInSession(added_player, session);
                    }
                }
            }
        }
    }

    std::string IncreaseTime(unsigned delta){
        std::string res =  game_handler_.IncreaseTime(delta, game_);
        /* 
            Сохраняем игровое состояние 
            синхроннно с ходом игровых часов только в том случае, 
            когда указан файл сохранения и период
        */
        if(state_save_.has_value()){
            state_save_.value().SaveOnTick(tick_period_.has_value());
        }
        return res;
    }

    void GenerateLoot(Milliseconds delta){
        return game_handler_.GenerateLoot(delta, game_);
    }

    std::string ApplyPlayerAction(const json::object& action, const Token& token){
        return game_handler_.SetAction(action, token);
    }

    std::string GetRecords(unsigned start, unsigned max_items){
        return game_handler_.GetRecords(start, max_items);
    }
private:
    Game& game_;
    Strand api_strand_;
    std::optional<unsigned> tick_period_;
    std::optional<GameStateSaveCase> state_save_;
    bool rand_spawn_;
    Players players_;
    PlayerTokens tokens_; 
    GameUseCase game_handler_;
    std::shared_ptr<detail::Ticker> time_ticker_;
    std::shared_ptr<detail::Ticker> loot_ticker_;
};

} // namespace app