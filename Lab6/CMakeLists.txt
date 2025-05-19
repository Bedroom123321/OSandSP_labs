cmake_minimum_required(VERSION 3.30)
project(Lab6)

set(CMAKE_CXX_STANDARD 20)

add_executable(Lab6 gen.c
        sort_index.c
        view.c)
