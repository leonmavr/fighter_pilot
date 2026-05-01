cmake_minimum_required(VERSION 3.24)

project(planet_aces VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLANET_ACES_BUILD_TESTS "Build tests" ON)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)

FetchContent_MakeAvailable(raylib)

add_library(planet_aces_sim
    src/aircraft/Aircraft.cpp
    src/aircraft/F15C.cpp
    src/assets/AircraftParametersAsset.cpp
    src/physics/FlightPhysics.cpp
)

target_include_directories(planet_aces_sim
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(planet_aces_sim PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(planet_aces_sim PUBLIC PLANET_ACES_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_executable(planet_aces_lab
    src/app/FlightLabApp.cpp
    src/fx/FxEntity.cpp
    src/fx/SmokeProfileAsset.cpp
    src/fx/SmokeTrail.cpp
    src/render/CameraController.cpp
    src/render/HudRenderer.cpp
    src/render/ObjModelRenderer.cpp
    src/render/RaylibContext.cpp
    src/main.cpp
    src/world/terrain.cpp
)

target_link_libraries(planet_aces_lab PRIVATE planet_aces_sim raylib)
target_compile_definitions(planet_aces_lab PRIVATE PLANET_ACES_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_include_directories(planet_aces_lab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(PLANET_ACES_BUILD_TESTS)
    include(CTest)

    add_executable(planet_aces_smoke
        tests/FlightModelSmoke.cpp
    )

    add_executable(planet_aces_aero_regression
        tests/FlightModelAeroRegression.cpp
    )

    target_link_libraries(planet_aces_smoke PRIVATE planet_aces_sim)
    target_link_libraries(planet_aces_aero_regression PRIVATE planet_aces_sim)
    add_test(NAME flight_model_smoke COMMAND planet_aces_smoke)
    add_test(NAME flight_model_aero_regression COMMAND planet_aces_aero_regression)
endif()
