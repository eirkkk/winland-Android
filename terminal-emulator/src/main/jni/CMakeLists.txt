cmake_minimum_required(VERSION 3.22.1)
project(termux C)

add_library(termux SHARED termux.c)
target_compile_options(termux PRIVATE -std=c11 -Wall -Wextra -Werror -Os -fno-stack-protector)
target_link_options(termux PRIVATE -Wl,--gc-sections)
