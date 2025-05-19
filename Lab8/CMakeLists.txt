cmake_minimum_required(VERSION 3.10)
project(lab8 C)

set(CMAKE_C_STANDARD 11)

add_executable(server server.c)
add_executable(client client.c)

target_link_libraries(server pthread)

# ./server /home/morra 8080
# ./client 127.0.0.1 8080