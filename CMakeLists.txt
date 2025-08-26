cmake_minimum_required(VERSION 4.0)
project(mysdlapps C)

set(CMAKE_C_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/sdk_dist/include)
link_directories(/usr/local/lib)

#add_executable(mysdlapps main_space_state.c)
#add_executable(mysdlapps main_snake.c)

# Desktop version
add_executable(mysdlapps main_random_app.c)
target_link_libraries(mysdlapps sdl3)

# WHY Badge version
add_executable(mysdlapps_badge main_random_app.c)
target_compile_definitions(mysdlapps_badge PRIVATE WHY_BADGE=1)
target_link_libraries(mysdlapps_badge sdl3)