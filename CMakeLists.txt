cmake_minimum_required(VERSION 3.0...4.0)
project(plea C)

set(CMAKE_C_STANDARD 99)

add_executable(plea src/plea.c src/lexer.c src/compiler.c src/vm.c)

install(TARGETS plea DESTINATION bin)