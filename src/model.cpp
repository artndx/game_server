#include "model.h"

#include <stdexcept>
#include <set>

namespace model {
using namespace std::literals;

namespace detail{

using namespace collision_detector;
/*
    Класс, предоставляющий всех собак и потерянных объектов
    в сессии для обработки коллизий

    Предметы — ширина ноль,
    Игроки — ширина 0,6,
    Базы — ширина 0,5.
*/

static const double LOOT_WIDTH = 0;
static const double DOG_WIDTH = 0.6;
static const double OFFICE_WIDTH = 0.5;

class ObjectsAndDogsProvider : public ItemGathererProvider{
public:
    using Objects = std::vector<Item>;
    using Dogs = std::vector<Gatherer>;

    ObjectsAndDogsProvider(Objects objects, Dogs dogs)
    : objects_(std::move(objects)), dogs_(std::move(dogs)){}

    size_t ItemsCount() const override{
        return objects_.size();
    }

    Item GetItem(size_t idx) const override{
        return objects_[idx];
    }

    size_t GatherersCount() const override{
        return dogs_.size();
    }

    Gatherer GetGatherer(size_t idx) const override{
        return dogs_[idx];
    }
private:
    Objects objects_;
    Dogs dogs_;
};

ObjectsAndDogsProvider::Objects MakeLoot(const std::list<Loot>& loots){
    ObjectsAndDogsProvider::Objects result;

    for(const Loot& loot : loots){
        result.emplace_back(loot.pos, LOOT_WIDTH);
    }

    return result;
}

ObjectsAndDogsProvider::Objects MakeOffices(const std::deque<Office>& offices){
    ObjectsAndDogsProvider::Objects result;

    for(const Office& office : offices){
        Point2D pos = {
            static_cast<double>(office.GetPosition().x), 
            static_cast<double>(office.GetPosition().y)
        };
        result.emplace_back(pos, OFFICE_WIDTH);
    }

    return result;
}

ObjectsAndDogsProvider::Dogs MakeDogs(const std::list<Dog>& dogs, double delta){
    ObjectsAndDogsProvider::Dogs result;

    for(const Dog& dog : dogs){
        PairDouble speed = *(dog.GetSpeed());

        Point2D start_pos = *(dog.GetPosition());
        Point2D end_pos = {start_pos.x + speed.x * delta, start_pos.y + speed.y * delta};

        result.emplace_back(start_pos, end_pos, DOG_WIDTH);
    }

    return result;
}

enum class GatheringEventType{
    DOG_COLLECT_ITEM,
    DOG_DELIVER_ALL_ITEMS
};

/*
    Смешивает события столкновений в хронологическом порядке
*/
using Event = std::pair<GatheringEvent, GatheringEventType>;
std::vector<Event> MixEvents(std::vector<GatheringEvent> collectings, std::vector<GatheringEvent> deliverings){
    std::vector<Event> result;
    size_t collectings_count = collectings.size();
    size_t deliverings_count = deliverings.size();
    
    result.resize(collectings_count + deliverings_count);

    /* Сначала размещаем все события, относящиеся к подбору объекта*/
    std::transform(collectings.cbegin(), collectings.cend(), result.begin(), [](const GatheringEvent& event){
        return Event{event, GatheringEventType::DOG_COLLECT_ITEM};
    });

    /* Потом размещаем события, в которым относит все объекты */
    std::transform(deliverings.cbegin(), deliverings.cend(), result.begin() + collectings_count , [](const GatheringEvent& event){
        return Event{event, GatheringEventType::DOG_DELIVER_ALL_ITEMS};
    });

    /* Сортируем в хронологическом порядке */
    std::sort(result.begin(), result.end(),[](const Event& lhs, const Event& rhs){
        return lhs.first.time < rhs.first.time;
    });

    return result;
}

} // namespace detail

/* ------------------------ Map ----------------------------------- */

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

const Map::LootTypes& Map::GetLootTypes() const noexcept{
    return loot_types_;
}

unsigned Map::GetRandomLootType() const{
    return GetRandomNumber(0, loot_types_.size());
}

void Map::AddRoad(const Road& road) {
    const Road& added_road = roads_.emplace_back(road);

    if(added_road.IsVertical()){
        road_map_[Map::RoadTag::VERTICAL].insert({road.GetStart().x, added_road});
    } else{
        road_map_[Map::RoadTag::HORIZONTAl].insert({road.GetStart().y, added_road});
    }
}

std::vector<const Road*> Map::FindRoadsByCoords(const Dog::Position& pos) const{
    std::vector<const Road*> result;
    FindInVerticals(pos, result);
    FindInHorizontals(pos, result);

    return result;
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Map::AddLootType(LootType loot_type){
    loot_types_.emplace_back(std::move(loot_type));
}

void Map::AddDogSpeed(double new_speed){
    dog_speed_ = new_speed;
}

double Map::GetDogSpeed() const{
    return dog_speed_;
}

void Map::AddBagCapacity(unsigned new_cap){
    bag_capacity_ = new_cap;
}

unsigned Map::GetBagCapacity() const{
    return bag_capacity_;
}

PairDouble Map::GetFirstPos(const model::Map::Roads& roads){
    const Point& pos = roads.begin()->GetStart();
    return {static_cast<double>(pos.x), static_cast<double>(pos.y)};
}

PairDouble Map::GetRandomPos(const model::Map::Roads& roads){
    model::Map::RoadTag tag = model::Map::RoadTag(GetRandomNumber(0, 2));
    size_t road_index = GetRandomNumber(0, roads.size());
    const model::Road& road = *(roads.begin());
    double x = 0;
    double y = 0;
    if(road.IsHorizontal()){
        x = GetRandomNumber(road.GetStart().x, road.GetEnd().x);
        y = road.GetStart().y;
    } else if(road.IsVertical()){
        x = road.GetStart().x;
        y = GetRandomNumber(road.GetStart().y, road.GetEnd().y);
    }
    return {x,y};
}

void Map::FindInVerticals(const Dog::Position& pos, std::vector<const Road*>& roads) const{
    const auto& v_roads = road_map_.at(Map::RoadTag::VERTICAL);
    ConstRoadIt it_x = v_roads.lower_bound((*pos).x);          /* Ищем ближайшую дорогу по полученной координате */

    if(it_x != v_roads.end()){                 
        if(it_x != v_roads.begin()){                   /* Если ближайшая дорога не является первой, то нужно проверить предыдущую */
            ConstRoadIt prev_it_x = std::prev(it_x, 1);
            if(CheckBounds(prev_it_x, pos)){
                roads.push_back(&prev_it_x->second);
            }
        }

        if(CheckBounds(it_x, pos)){                     /* Проверяем ближайшую дорогу */
            roads.push_back(&it_x->second);
        }
    } else {                                            /* Если дорога не найдена, то полученная координата дальше всех дорог*/
        it_x = std::prev(v_roads.end(), 1);            /* Тогда проверяем последнюю дорогу */
        if(CheckBounds(it_x, pos)){
            roads.push_back(&it_x->second);
        }
    }
}

void Map::FindInHorizontals(const Dog::Position& pos, std::vector<const Road*>& roads) const{
    const auto& h_roads = road_map_.at(Map::RoadTag::HORIZONTAl);
    ConstRoadIt it_y = h_roads.lower_bound((*pos).y);          /* Ищем ближайшую дорогу по полученной координате */

    if(it_y != h_roads.end()){                 
        if(it_y != h_roads.begin()){                   /* Если ближайшая дорога не является первой, то нужно проверить предыдущую */
            ConstRoadIt prev_it_y = std::prev(it_y, 1);
            if(CheckBounds(prev_it_y, pos)){
                roads.push_back(&prev_it_y->second);
            }
        }

        if(CheckBounds(it_y, pos)){                     /* Проверяем ближайшую дорогу */
            roads.push_back(&it_y->second);
        }
    } else {                                            /* Если дорога не найдена, то полученная координата дальше всех дорог*/
        it_y = std::prev(h_roads.end(), 1);            /* Тогда проверяем последнюю дорогу */
        if(CheckBounds(it_y, pos)){
            roads.push_back(&it_y->second);
        }
    }
}

bool Map::CheckBounds(ConstRoadIt it, const Dog::Position& pos) const{
    Point start = it->second.GetStart();
    Point end = it->second.GetEnd();
    if(it->second.IsInvert()){
        std::swap(start, end);
    }
    return ((start.x - 0.4 <= (*pos).x && (*pos).x <= end.x + 0.4) && 
                (start.y - 0.4 <= (*pos).y && (*pos).y <= end.y + 0.4));
}
/* ------------------------ GameSession ----------------------------------- */

Dog* GameSession::AddDog(int id, const Dog::Name& name, 
                    const Dog::Position& pos, const Dog::Speed& vel, 
                    Direction dir){
    dogs_.emplace_back(id, name, pos, vel, dir);
    return &dogs_.back();
}

Dog* GameSession::AddCreatedDog(Dog new_dog){
    dogs_.emplace_back(std::move(new_dog));
    return &dogs_.back();
}

const Map* GameSession::GetMap() const {
    return map_;
}

std::list<Dog>& GameSession::GetDogs(){
    return dogs_;
}

const std::list<Dog>& GameSession::GetDogs() const{
    return static_cast<const std::list<Dog>&>(dogs_);
}

void GameSession::UpdateLoot(unsigned loot_count){
    for(unsigned i = 0; i < loot_count; ++i){
        unsigned type = map_->GetRandomLootType();
        PairDouble pos = Map::GetRandomPos(map_->GetRoads());
        unsigned value = 1;

        const LootType& loot_type = map_->GetLootTypes().at(type);
        if(loot_type.value.has_value()){
            value = map_->GetLootTypes().at(type).value.value();
        }
        loot_.emplace_back(++auto_loot_counter_, type, value, pos);
    }
}

void GameSession::SetLootObjects(std::list<Loot> new_loot){
    loot_ = std::move(new_loot);
}

const std::list<Loot>& GameSession::GetLootObjects() const{
    return loot_;
}

void GameSession::DeleteCollectedLoot(const std::set<size_t>& collected_items){
    for(auto collect_id = collected_items.rbegin(); collect_id != collected_items.rend(); std::advance(collect_id, 1)){
        auto it = std::next(loot_.begin(), *collect_id);
        loot_.erase(it);
    }
}

void GameSession::DeleteDog(const Dog* erasing_dog){
    auto it = std::find_if(dogs_.begin(), dogs_.end(), [erasing_dog](const Dog& dog){
        return &dog == erasing_dog;
    });

    dogs_.erase(it);
}

/* ------------------------ Game ----------------------------------- */

void Game::AddMap(Map&& map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

GameSession* Game::AddSession(const Map::Id& map_id){
    if(const Map* map = FindMap(map_id); map != nullptr){
        GameSession* session = &(map_id_to_sessions_[map_id].emplace_back(map));
        return session;
    }
    return nullptr;
}

GameSession* Game::SessionIsExists(const Map::Id& map_id){
    if(const Map* map = FindMap(map_id); map != nullptr){
        if(!map_id_to_sessions_[map_id].empty()){
            GameSession* session = &(map_id_to_sessions_[map_id].back());
            return session;
        }
    }
    return nullptr;
}

const Game::SessionsByMapId& Game::GetAllSessions() const{
    return map_id_to_sessions_;
}

void Game::SetLootGenerator(double period, double probability){
    loot_generator_.emplace(detail::FromDouble(period), probability);
}

void Game::SetDefaultDogSpeed(double new_speed){
    default_dog_speed_ = new_speed;
}

double Game::GetDefaultDogSpeed() const{
    return default_dog_speed_;
}

void Game::SetDefaultBagCapacity(unsigned new_cap){
    default_bag_capacity_ = new_cap;
}

unsigned Game::GetDefaultBagCapacity() const{
    return default_bag_capacity_;
}

void Game::SetDogRetirementTime(unsigned dog_retirement_time){
    dog_retirement_time_ = dog_retirement_time;
}
    
unsigned Game::GetDogRetirementTime() const{
    return dog_retirement_time_;
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

detail::Milliseconds Game::GetLootGeneratePeriod() const{
    return loot_generator_.value().GetPeriod();
}

void Game::GenerateLootInSessions(detail::Milliseconds delta){
    for(auto& [map_id, sessions] : map_id_to_sessions_){
        for(GameSession& session : sessions){
            unsigned current_loot_count = session.GetLootObjects().size();
            unsigned loot_count = (*loot_generator_).Generate(delta, current_loot_count, session.GetDogs().size());
            session.UpdateLoot(loot_count);
        }
    }
}

void Game::UpdateGameState(unsigned delta){
    double delta_in_seconds = static_cast<double>(delta) / 1000;
    for(auto& [map_id, sessions] : map_id_to_sessions_){
        for(GameSession& session : sessions){
            UpdateDogsLoot(session, delta_in_seconds);
            UpdateAllDogsPositions(session.GetDogs(), session.GetMap(), delta_in_seconds);
        }
    }
}

void Game::DisconnectDogFromSession(const GameSession* player_session, const Dog* erasing_dog){
    Map::Id map_id = player_session->GetMap()->GetId();

    std::deque<GameSession>& sessions = map_id_to_sessions_.at(map_id);
    auto it = std::find_if(sessions.begin(), sessions.end(), [player_session](const GameSession& session){
        return &session == player_session;
    });

    GameSession& found_session = *it;
    found_session.DeleteDog(erasing_dog);
}

void Game::UpdateAllDogsPositions(std::list<Dog>& dogs, const Map* map, double delta){
    for(Dog& dog : dogs){
        std::vector<const Road*> roads = map->FindRoadsByCoords(dog.GetPosition());
        UpdateDogPos(dog, roads, delta);
    }
}

void Game::UpdateDogPos(Dog& dog, const std::vector<const Road*>& roads, double delta){
    const auto [x, y] = *(dog.GetPosition());
    const auto [vx, vy] = *(dog.GetSpeed());

    const PairDouble getting_pos({x + vx * delta, y + vy * delta});
    const PairDouble getting_speed({vx, vy});

    PairDouble result_pos(getting_pos);
    PairDouble result_speed(getting_speed);

    std::set<PairDouble> collisions;

    for(const Road* road : roads){
        Point start = road->GetStart();
        Point end = road->GetEnd();

        if(road->IsInvert()){
            std::swap(start, end);
        }

        if(IsInsideRoad(getting_pos, start, end)){
            dog.SetPosition(Dog::Position(getting_pos));
            dog.SetSpeed(Dog::Speed(getting_speed));
            return;
        }

        if(start.x - 0.4 >= getting_pos.x) {
            result_pos.x = start.x - 0.4;
        } else if(getting_pos.x >= end.x + 0.4){
            result_pos.x = end.x + 0.4;
        }

        if(start.y - 0.4 >= getting_pos.y) {
            result_pos.y = start.y - 0.4;
        } else if(getting_pos.y >= end.y + 0.4){
            result_pos.y = end.y + 0.4;
        }

        collisions.insert(result_pos);
    }

    if(collisions.size() != 0){
        result_pos = *(std::prev(collisions.end(), 1));
        result_speed = {0,0};
    }
    
    dog.SetPosition(Dog::Position(result_pos));
    dog.SetSpeed(Dog::Speed(result_speed));
}   

void Game::UpdateDogsLoot(GameSession& session, double delta) {
    using namespace collision_detector;
    std::list<Dog>& dogs = session.GetDogs();
    const std::list<Loot>& all_loots = session.GetLootObjects();
    unsigned max_bag_capacity = session.GetMap()->GetBagCapacity();
    const std::deque<Office>& offices = session.GetMap()->GetOffices();

    /* Провайдер для предоставления событий при подборе предметов*/
    detail::ObjectsAndDogsProvider loots_provider(detail::MakeLoot(all_loots), detail::MakeDogs(dogs, delta));

    /* Провайдер для предоставления событий при доставке в офис */
    detail::ObjectsAndDogsProvider offices_provider(detail::MakeOffices(offices), detail::MakeDogs(dogs, delta));
    auto events = detail::MixEvents(FindGatherEvents(loots_provider), FindGatherEvents(offices_provider));
    std::set<size_t> collected_loot;
    for(const auto& [event, event_type] : events){
        auto dog_it = std::next(dogs.begin(), event.gatherer_id);
        Dog& dog = *dog_it;
        switch (event_type){
            case detail::GatheringEventType::DOG_COLLECT_ITEM:
                // Собака подбирает предмет
                // если её рюкзак не полон
                if((*dog.GetBag()).size() < max_bag_capacity){
                    // если до этого этот предмет не подбирали
                    if(!collected_loot.count(event.item_id)){
                        auto loot_it = std::next(all_loots.begin(), event.item_id);
                        const Loot& loot = *loot_it;
                        dog.CollectItem(loot);
                        collected_loot.insert(event.item_id);
                    }
                }
                break;
            case detail::GatheringEventType::DOG_DELIVER_ALL_ITEMS:
                dog.ClearBag();
                break;
        
            default:
                break;
        }
    }

    /* Подобранные предметы должны пропасть с карты*/
    session.DeleteCollectedLoot(collected_loot);
}

bool Game::IsInsideRoad(const PairDouble& getting_pos, const Point& start, const Point& end){
    bool in_left_border = getting_pos.x >= start.x - road_offset_;
    bool in_right_border = getting_pos.x <= end.x + road_offset_;
    bool in_upper_border = getting_pos.y >= start.y - road_offset_;
    bool in_lower_border = getting_pos.y <= end.y + road_offset_;

    return in_left_border && in_right_border && in_upper_border && in_lower_border;
}


}  // namespace model
