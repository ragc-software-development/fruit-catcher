add_library(${PROJECT_NAME}Common STATIC
    src/types.cpp
)

target_include_directories(${PROJECT_NAME}Common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}Common PUBLIC ${PROJECT_NAME}_options)