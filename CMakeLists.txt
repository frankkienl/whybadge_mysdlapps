cmake_minimum_required(VERSION 4.0)
project(mysdlapps C)

set(CMAKE_C_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/sdk_dist/include)
link_directories(/usr/local/lib)

#add_executable(mysdlapps main_space_state.c)
#add_executable(mysdlapps main_random_app.c)
add_executable(mysdlapps main_snake.c)
target_link_libraries(mysdlapps sdl3)