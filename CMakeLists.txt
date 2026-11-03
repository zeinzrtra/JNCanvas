cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

add_compile_definitions(BUILDING_JN_CANVAS=1)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_include_directories(${CMAKE_PROJECT_NAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  src/vertical-canvas.cpp
  src/vertical-canvas.hpp
  src/scenes-dock.cpp
  src/scenes-dock.hpp
  src/sources-dock.cpp
  src/sources-dock.hpp
  src/source-tree.cpp
  src/source-tree.hpp
  src/transitions-dock.cpp
  src/transitions-dock.hpp
  src/qt-display.cpp
  src/qt-display.hpp
  src/projector.cpp
  src/projector.hpp
  src/name-dialog.cpp
  src/name-dialog.hpp
  src/display-helpers.hpp
  src/multi-canvas-source.c
  src/multi-canvas-source.h
  src/audio-wrapper-source.c
  src/audio-wrapper-source.h
  src/obs-websocket-api.h
  src/version.h
  src/ui/canvas-central-view.cpp
  src/ui/canvas-central-view.hpp
  src/ui/canvas-view-dock.cpp
  src/ui/canvas-view-dock.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
