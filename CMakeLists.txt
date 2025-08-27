cmake_minimum_required(VERSION 4.0)
project(mysdlapps C)

set(CMAKE_C_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/sdk_dist/include)
link_directories(/usr/local/lib)

### RandomApp
# Desktop version
add_executable(randomapp main_random_app.c)
target_link_libraries(randomapp sdl3)
# WHY Badge version
add_executable(randomapp_badge main_random_app.c)
target_compile_definitions(randomapp_badge PRIVATE WHY_BADGE=1)
target_link_libraries(randomapp_badge sdl3)

### Space State NL
# Desktop version
add_executable(spacestatenl spacestate_nl/main_space_state.c)
target_link_libraries(spacestatenl sdl3 curl)
# WHY Badge version
add_executable(spacestatenl_badge spacestate_nl/main_space_state.c)
target_compile_definitions(spacestatenl_badge PRIVATE WHY_BADGE=1)
target_link_libraries(spacestatenl_badge sdl3 curl)