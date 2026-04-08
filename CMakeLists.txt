cmake_minimum_required(VERSION 3.20)
project(codebeat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CODEBEAT_BUILD_GUI "Build Qt GUI shell" ON)

add_library(codebeat_core INTERFACE)
target_include_directories(codebeat_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(codebeat_train training/lm_train.cpp)
target_link_libraries(codebeat_train PRIVATE codebeat_core)

add_executable(codebeat_repl runtime/repl.cpp)
target_link_libraries(codebeat_repl PRIVATE codebeat_core)

if(CODEBEAT_BUILD_GUI)
  find_package(Qt6 COMPONENTS Widgets QUIET)
  if(Qt6_FOUND)
    qt_standard_project_setup()
    qt_add_executable(codebeat_gui
      runtime/gui_main.cpp
      runtime/mainwindow.cpp
      runtime/mainwindow.hpp
      runtime/splash.cpp
      runtime/splash.hpp
    )
    target_link_libraries(codebeat_gui PRIVATE Qt6::Widgets codebeat_core)
    target_compile_definitions(codebeat_gui PRIVATE CODEBEAT_HAS_QT=1)
  else()
    message(WARNING "Qt6 Widgets not found. GUI target 'codebeat_gui' will not be built.")
  endif()
endif()
