cmake_minimum_required(VERSION 3.2)
project(LaserTagClient)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(Boost_INCLUDE_DIR /usr/local/include)
set(Boost_LIBRARY_DIR /usr/local/lib)
find_package(Boost REQUIRED COMPONENTS system)

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})

include_directories(../game)

set(CLIENT_SOURCE_FILES main.cpp client.cpp ui.cpp ../game/player.cpp ../game/geometry.cpp)
add_executable(LaserTagClient ${CLIENT_SOURCE_FILES})
target_link_libraries(LaserTagClient ${Boost_LIBRARIES} ${OPENGL_LIBRARIES} ${GLUT_LIBRARY})
