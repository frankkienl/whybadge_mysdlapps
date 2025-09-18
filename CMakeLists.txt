cmake_minimum_required(VERSION 4.0)

option(BUILD_EMSCRIPTEN "Build with Emscripten" OFF)

if (BUILD_EMSCRIPTEN)
    # Reset some CMake variables that interfere with Emscripten
    set(CMAKE_C_FLAGS "" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "" CACHE STRING "" FORCE)
    # set(CMAKE_C_COMPILER "emcc")
    # set(CMAKE_CXX_COMPILER "em++")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -sUSE_SDL=3")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_SDL=3")

    # Remove any -arch arm64 flags that might be set by default on macOS
    set(CMAKE_OSX_ARCHITECTURES "" CACHE STRING "" FORCE)
    foreach (lang C CXX)
        string(REPLACE "-arch arm64" "" CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}")
        set(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}" CACHE STRING "" FORCE)
    endforeach ()

    # other emscripten flags if needed:
    # add_link_options(-sALLOW_MEMORY_GROWTH -sASSERTIONS)
endif ()

project(mysdlapps C)

set(CMAKE_C_STANDARD 17)

# Vendored libraries (SDL3, SDL3_image)
# These are commented out because we are using the installed SDL3 version on the system
# They are in: /usr/local/include
#add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
#add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)

### RandomApp
# Emscripten version
if (BUILD_EMSCRIPTEN)
    add_executable(randomapp_emscripten randomapp/main_random_app.c)
    target_include_directories(randomapp_emscripten PRIVATE ${CMAKE_SOURCE_DIR}/sdk_dist/include)
    target_link_libraries(randomapp_emscripten sdl3)
endif ()

link_directories(/usr/local/lib)
include_directories(${CMAKE_SOURCE_DIR}/sdk_dist/include)

# Desktop version
add_executable(randomapp randomapp/main_random_app.c)
target_link_libraries(randomapp sdl3)
# WHY Badge version
add_executable(randomapp_badge randomapp/main_random_app.c)
target_compile_definitions(randomapp_badge PRIVATE WHY_BADGE=1)
target_link_libraries(randomapp_badge sdl3)

### Space State NL
# Desktop version
add_executable(spacestatenl spacestate_nl/main_space_state.c
        spacestate_nl/stb_image.c)
target_link_libraries(spacestatenl sdl3 curl)
# WHY Badge version
add_executable(spacestatenl_badge spacestate_nl/main_space_state.c
        spacestate_nl/stb_image.c)
target_compile_definitions(spacestatenl_badge PRIVATE WHY_BADGE=1)
target_link_libraries(spacestatenl_badge sdl3 curl)