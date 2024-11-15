#pragma once
#include <random>
#include "model.h"

namespace util {

using DogMapKey = std::pair<int, model::Map::Id>;
struct DogMapKeyHasher{
    size_t operator()(const DogMapKey& value) const;
};

} // namespace util

namespace model{

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;


class Players;
class PlayerTokens;

/* ------------------------ Player ----------------------------------- */

class Player{
public:
    using Name = util::Tagged<std::string, Player>;
    int GetId() const{
        return id_;
    }

    Name GetName() const{
        return name_;
    }

    Dog* GetDog(){
        return dog_;
    }

    void SetPlayerTimeClock(const Dog::SpeedSignal::slot_type& slot) const {
        dog_->SetSlotSpeed(slot);
    }

    const Dog* GetDog() const{
        return static_cast<const Dog*>(dog_);
    }

    const GameSession* GetSession() const{
        return session_;
    }

    void SetToken(Token token){
        token_ = token;
    }

    const Token& GetToken() const {
        return token_;
    }
private:
    friend PlayerTokens;
    friend Players;

    Player(int id, Name name, Dog* dog, const GameSession* session)
        : id_(id), name_(name), dog_(dog), token_(""),session_(session){
    }

    int id_;
    Name name_;
    Token token_;
    Dog* dog_;
    const GameSession* session_;
};

/* ------------------------ Players ----------------------------------- */

class Players{
public:
    using PlayerList = std::unordered_map<util::DogMapKey, Player, util::DogMapKeyHasher>;
    Players() = default;

    Player& Add(int id, const Player::Name& name, Dog* dog, const GameSession* session);

    const Player* FindByDogIdAndMapId(int dog_id, std::string map_id) const;

    const PlayerList& GetPlayers() const;

    void DeletePlayer(const Player* erasing_player);
private:
    PlayerList players_;
};

/* ---------------------- PlayerTokens ------------------------------------- */

class PlayerTokens{
public:
    using PlayersInSession = std::deque<const Player*>;
    using TokenToPlayer = std::unordered_map<Token, Player*, util::TaggedHasher<Token>>;
    using SessionToPlayers = std::unordered_map<const model::GameSession*, PlayersInSession>;
    PlayerTokens() = default;

    Token AddPlayer(Player& player);

    void AddPlayerWithToken(Player& player, Token token);

    void AddPlayerInSession(Player& player, const GameSession* session);

    Player* FindPlayerByToken(const Token& token); 

    const Player* FindPlayerByToken(const Token& token) const;

    const PlayersInSession& GetPlayersBySession(const GameSession* session) const;

    const TokenToPlayer& GetAllTokens() const;

    void DeletePlayer(const Player* erasing_player);
private:
    Token GenerateToken();
    std::random_device random_device_;

    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    // Чтобы сгенерировать токен, получите из generator1_ и generator2_
    // два 64-разрядных числа и, переведя их в hex-строки, склейте в одну.
    // Вы можете поэкспериментировать с алгоритмом генерирования токенов,
    // чтобы сделать их подбор ещё более затруднительным
    TokenToPlayer token_to_player_;
    SessionToPlayers players_by_session_;
};

} // namespace model