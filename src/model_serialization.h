#pragma once
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/unordered_map.hpp>


#include "player.h"

namespace model {

template <typename Archive>
void serialize(Archive& ar, model::PairDouble& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Loot& obj, [[maybe_unused]] const unsigned version) {
    ar&(obj.id);
    ar&(obj.type);
    ar&(obj.value);
    ar&(obj.pos);
}

template <typename Archive>
void serialize(Archive& ar, Map::Id& map_id, [[maybe_unused]] const unsigned version) {
    ar&(*map_id);
}

}  // namespace model

namespace serialization {

using namespace model;

class PlayerRepr{
public:
    PlayerRepr()
    :id_(0), name_(), token_(""){}

    PlayerRepr(const Player* player)
    :id_(player->GetId()), name_(*(player->GetName())), token_(*(player->GetToken())){}

    int GetId() const{
        return id_;
    }

    std::string GetName() const{
        return name_;
    }

    std::string GetToken() const{
        return token_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& token_;
    }
private:
    int id_;
    std::string name_;
    std::string token_;
};

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const Dog& dog)
        : id_(dog.GetId())
        , name_(*(dog.GetName()))
        , pos_(*(dog.GetPosition()))
        , speed_(*(dog.GetSpeed()))
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_(*(dog.GetBag()))
        , player_repr_(){
    }

    void AddPlayerRepr(PlayerRepr pr){
        player_repr_ = std::move(pr);
    }
    
    const PlayerRepr& GetPlayerRepr() const{
        return player_repr_;
    }

    [[nodiscard]] Dog Restore() const {
        Dog dog(id_, Dog::Name(name_), Dog::Position(pos_), Dog::Speed(speed_), direction_);
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& pos_;
        ar& speed_;
        ar& direction_;
        ar& score_;
        ar& bag_;
        ar& player_repr_;
    }

private:
    int id_ = 0u;
    std::string name_;
    PairDouble pos_;
    PairDouble speed_;
    Direction direction_ = Direction::NORTH;
    unsigned score_ = 0;
    std::deque<Loot> bag_;
    PlayerRepr player_repr_;
};

class SessionRepr{
public:

    SessionRepr() = default;

    void AddLoots(const std::list<Loot>& loot){
        loot_ = loot;
    }

    const std::list<Loot>& GetLoot() const{
        return loot_;
    }

    void AddDogsRepr(std::list<DogRepr> dogs_repr){
        dogs_repr_ = std::move(dogs_repr);
    }

    const std::list<DogRepr>& GetDogsRepr() const{
        return dogs_repr_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& loot_;
        ar& dogs_repr_;
    }
private:
    std::list<Loot> loot_;
    std::list<DogRepr> dogs_repr_;
};

class GameStateRepr{
public:
    using SessionsByMapId = std::unordered_map<std::string, std::deque<SessionRepr>>;
    GameStateRepr() = default;

    GameStateRepr(const Game::SessionsByMapId& sessions_by_map, const Players& players){
        for(const auto& [map_id, sessions] : sessions_by_map){
            std::list<Loot> loot;
            std::list<DogRepr> dogs_repr;
            for(const auto& session : sessions){
                loot = session.GetLootObjects();
                for(const auto& dog : session.GetDogs()){
                    dogs_repr.emplace_back(DogRepr(dog));

                    const Player* player = players.FindByDogIdAndMapId(dog.GetId(), *map_id);
                    PlayerRepr player_repr(player);
                    dogs_repr.back().AddPlayerRepr(player_repr);
                }
            }

            SessionRepr session_repr;
            session_repr.AddLoots(loot);
            session_repr.AddDogsRepr(dogs_repr);

            all_sessions_[*map_id].emplace_back(std::move(session_repr));
        }
    }

    const SessionsByMapId& GetAllSessions() const{
        return all_sessions_;
    }
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& all_sessions_;
    }
    
private:
    SessionsByMapId all_sessions_;
};

}  // namespace serialization
