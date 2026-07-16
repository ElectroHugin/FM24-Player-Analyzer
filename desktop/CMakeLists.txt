cmake_minimum_required(VERSION 3.28)

project(fm24analyzer
    VERSION 1.3.0
    DESCRIPTION "FM24 Player Analyzer - native desktop app"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets Charts Concurrent Test LinguistTools)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(src/tools)
add_subdirectory(src/tests)
