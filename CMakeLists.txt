cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Создание библиотеки модели
add_library(game_model STATIC
	src/model.cpp src/model.h
	src/loot_generator.cpp src/loot_generator.h
	src/model_serialization.h
	src/tagged.h
	src/geom.h
)
target_link_libraries(game_model PUBLIC CONAN_PKG::boost Threads::Threads)

# Создание библиотеки модуля обработчика коллизий
add_library(collision_detection_lib STATIC
	src/collision_detector.h
	src/collision_detector.cpp
	src/geom.h
)
target_link_libraries(collision_detection_lib PUBLIC CONAN_PKG::boost Threads::Threads)

# Создание основного приложения
add_executable(game_server 
	src/main.cpp
	src/cmd_parser.cpp src/cmd_parser.h
	src/http_server.cpp src/http_server.h
	src/sdk.h 
	src/tagged.h
	src/boost_json.cpp
	src/json_loader.h src/json_loader.cpp
	src/request_handler.cpp src/request_handler.h
	src/player.cpp src/player.h
	src/connection_pool.cpp src/connection_pool.h
	src/app.cpp src/app.h
	src/logger.cpp src/logger.h
)
target_link_libraries(game_server game_model collision_detection_lib CONAN_PKG::libpqxx)


# add_executable(game_server_tests
# 	tests/state-serialization-tests.cpp
# )

# target_link_libraries(game_server_tests CONAN_PKG::catch2 game_model)
