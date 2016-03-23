cmake_minimum_required(VERSION 3.2)
project(TeamBattle)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(Boost_INCLUDE_DIR /usr/local/include)
set(Boost_LIBRARY_DIR /usr/local/lib)
find_package(Boost REQUIRED COMPONENTS system)

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})

set(SERVER_SOURCE_FILES server.cpp)
set(CLIENT_SOURCE_FILES client.cpp)
add_executable(TeamBattleServer ${SERVER_SOURCE_FILES})
add_executable(TeamBattleClient ${CLIENT_SOURCE_FILES})
target_link_libraries(TeamBattleServer ${Boost_LIBRARIES})
target_link_libraries(TeamBattleClient ${Boost_LIBRARIES})
