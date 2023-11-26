cmake_minimum_required(VERSION 3.0.0)
project(cantserver VERSION 0.1.0 LANGUAGES C)

add_executable(cantserver main.c)
target_link_libraries(cantserver websockets)
