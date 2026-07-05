if (APPLE)
    set(NETWORK_IMPLEMENTATION src/network_acceptor_apple.cpp)
    message(STATUS "Configurando Rede do Servidor: Backend Nativo macOS (kqueue)")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR LINUX)
    set(NETWORK_IMPLEMENTATION src/network_acceptor_linux.cpp)
    message(STATUS "Configurando Rede do Servidor: Backend Nativo Linux (epoll)")
endif()

add_executable(${PROJECT_NAME}Server
    src/main.cpp
    ${NETWORK_IMPLEMENTATION}
)

target_include_directories(${PROJECT_NAME}Server PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${PROJECT_NAME}Server PRIVATE ${PROJECT_NAME}Common ${PROJECT_NAME}_options)