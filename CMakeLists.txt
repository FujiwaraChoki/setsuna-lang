cmake_minimum_required(VERSION 3.16)
project(setsuna VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(setsuna
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/environment.cpp
    src/evaluator.cpp
    src/builtins.cpp
    src/typechecker.cpp
)

target_include_directories(setsuna PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(setsuna PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Debug/Release
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(setsuna PRIVATE DEBUG)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS setsuna RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES stdlib/prelude.stsn DESTINATION ${CMAKE_INSTALL_DATADIR}/setsuna)
