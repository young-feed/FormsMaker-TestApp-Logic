cmake_minimum_required(VERSION 3.15)
project(PostgresMainModule)

set(CMAKE_CXX_STANDARD 17)

# Поиск зависимостей через vcpkg
find_package(libpq REQUIRED)         # Базовая библиотека C
find_package(nlohmann_json REQUIRED)
find_package(jwt-cpp REQUIRED)

add_executable(main_module main.cpp)

# Линкуем libpq и другие библиотеки
target_link_libraries(main_module PRIVATE 
    ${PostgreSQL_LIBRARIES} 
    nlohmann_json::nlohmann_json 
    jwt-cpp::jwt-cpp
)

# Добавляем пути к заголовочным файлам PostgreSQL
target_include_directories(main_module PRIVATE ${PostgreSQL_INCLUDE_DIRS})