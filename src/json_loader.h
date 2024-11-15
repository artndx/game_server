#pragma once
#include <boost/json.hpp>
#include <filesystem>
#include "model.h"

namespace json_loader {

namespace json = boost::json;
using namespace model;

int GetInt(std::string key, const json::object& obj);

std::string GetString(std::string key, const json::object& obj);

void AddRoadsFromJson(const json::object& json_map, Map& map);

void AddBuildingsFromJson(const json::object& json_map, Map& map);

void AddOfficesFromJson(const json::object& json_map, Map& map);

void AddLootTypesFromJson(const json::object& json_map, Map& map);

void LoadConfig(std::string json_str, Game& game);

void AddMaps(const json::array& json_maps, Game& game);

model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
