cmake_minimum_required(VERSION 3.16)
project(YourLANParty C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/common ${CMAKE_SOURCE_DIR}/db ${CMAKE_SOURCE_DIR}/third_party/sqlite)

#SQLite is plain C code, so it gets built as its own small library first
add_library(sqlite3_lib STATIC third_party/sqlite/sqlite3.c)

add_executable(server server/server_main.cpp server/Server.cpp db/Database.cpp)
add_executable(client client/client_main.cpp)
add_executable(db_test db/db_test.cpp db/Database.cpp)

target_link_libraries(server Threads::Threads sqlite3_lib)
target_link_libraries(client Threads::Threads)
target_link_libraries(db_test Threads::Threads sqlite3_lib)

if(WIN32)
    target_link_libraries(server ws2_32)
    target_link_libraries(client ws2_32)
    target_link_options(server PRIVATE -static)
    target_link_options(client PRIVATE -static)
endif()