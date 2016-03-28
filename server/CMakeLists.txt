cmake_minimum_required(VERSION 3.2)
project(LaserTagServer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(BOOST_ROOT /usr/local/)
find_package(Boost REQUIRED COMPONENTS system)

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})

include_directories(../game)

set(SERVER_SOURCE_FILES main.cpp server.cpp session.cpp ../game/player.cpp ../game/geometry.cpp)
add_executable(LaserTagServer ${SERVER_SOURCE_FILES})
target_link_libraries(LaserTagServer ${Boost_LIBRARIES})
