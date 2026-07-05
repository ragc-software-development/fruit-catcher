add_executable(${PROJECT_NAME}Client
    src/main.cpp
)

target_include_directories(${PROJECT_NAME}Client PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(${PROJECT_NAME}Client PRIVATE ${PROJECT_NAME}Common ${PROJECT_NAME}_options)