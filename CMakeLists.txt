cmake_minimum_required(VERSION 3.16...3.28)

project(jn-canvas VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_compile_definitions(BUILDING_JN_CANVAS=1)

# Find Qt 6
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Find OBS
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)

set(SOURCES
    src/plugin-main.cpp
    src/canvas-engine.cpp
    src/record-engine.cpp
    src/ui/canvas-preview-dock.cpp
    src/ui/canvas-control-dock.cpp
)

set(HEADERS
    include/jn-canvas-export.h
    src/canvas-engine.hpp
    src/record-engine.hpp
    src/ui/canvas-preview-dock.hpp
    src/ui/canvas-control-dock.hpp
)

add_library(jn-canvas MODULE
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(jn-canvas
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(jn-canvas
    PRIVATE
        OBS::libobs
        OBS::obs-frontend-api
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

# Output directory and naming
set_target_properties(jn-canvas PROPERTIES
    PREFIX ""
    OUTPUT_NAME "jn-canvas"
)

if(WIN32)
    set_target_properties(jn-canvas PROPERTIES
        SUFFIX ".dll"
    )
endif()

# Installation target
install(TARGETS jn-canvas
    LIBRARY DESTINATION obs-plugins/64bit
    RUNTIME DESTINATION obs-plugins/64bit
)
