cmake_minimum_required(VERSION 3.21)
project(IndiaDevelopmentSimulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(ISIM_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network Sql Charts Concurrent)

if(MSVC)
  add_compile_options(/W3 /utf-8 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

add_subdirectory(src)

if(ISIM_BUILD_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  enable_testing()
  add_subdirectory(tests)
endif()
