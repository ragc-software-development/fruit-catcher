cmake_minimum_required(VERSION 3.22)

project(FruitCatcher 
    VERSION 1.0
    DESCRIPTION "A simple fruit catching game"
    LANGUAGES CXX
)

add_library(${PROJECT_NAME}_options INTERFACE)

target_compile_features(${PROJECT_NAME}_options INTERFACE cxx_std_20)

target_compile_options(${PROJECT_NAME}_options INTERFACE
    $<$<CONFIG:Release>:-O3 -march=native -fno-plt -fno-semantic-interposition>
)

target_link_options(${PROJECT_NAME}_options INTERFACE
    $<$<CONFIG:Release>:-flto -fuse-ld=gold>
)

target_compile_options(${PROJECT_NAME}_options INTERFACE
    $<$<CONFIG:Debug>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>
)

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)