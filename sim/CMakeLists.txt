cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(Simulation)

find_package(Geant4 REQUIRED ui_all vis_all)
include(${Geant4_USE_FILE})

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB sources ${PROJECT_SOURCE_DIR}/source/*.cc)

add_executable(sim sim.cc ${sources})

target_link_libraries(sim ${Geant4_LIBRARIES})

set_target_properties(sim PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)