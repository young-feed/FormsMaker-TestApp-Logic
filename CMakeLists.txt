cmake_minimum_required(VERSION 3.15)
project(MainModule)

set(CMAKE_CXX_STANDARD 17)

# Явно указываем пути к вашей установленной PostgreSQL
set(POSTGRES_PATH "C:/Program Files/PostgreSQL/18")

include_directories("${POSTGRES_PATH}/include")
link_directories("${POSTGRES_PATH}/lib")

# Поиск nlohmann_json и jwt-cpp через vcpkg
find_package(nlohmann_json REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(httplib REQUIRED)

add_executable(main_module main.cpp)

# Линкуем библиотеки
target_link_libraries(main_module PRIVATE 
    libpq 
    nlohmann_json::nlohmann_json 
    jwt-cpp::jwt-cpp
    httplib::httplib
)

add_custom_command(TARGET main_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PostgreSQL_ROOT}/bin/libpq.dll"
    $<TARGET_FILE_DIR:main_module>
)