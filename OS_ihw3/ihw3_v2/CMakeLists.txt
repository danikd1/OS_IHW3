cmake_minimum_required(VERSION 3.26)
project(ihw3_v2 C)

set(CMAKE_C_STANDARD 11)

add_executable(ihw3_v2 main.c
        client.c
        monitor.c)
