cmake_minimum_required(VERSION 3.8)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Source Files
set(SOURCES
    src/main.c
)

# Libraries
set(LIBRARIES
    websockets
    m
)

# Project
project(cantClient VERSION 0.1.0 LANGUAGES C CXX)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC "src/include")

set_target_properties(${PROJECT_NAME} PROPERTIES WORKING_DIRECTORY "res")
