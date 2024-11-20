cmake_minimum_required(VERSION 3.20)

set(DEMO_PROJECT_NAME "VkHydroCoreDemo")
set(TEST_RESOURCE_PATH "${CMAKE_CURRENT_BINARY_DIR}/res" CACHE STRING "Path to test resources")

# Configure resource file
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/TestConfig.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/include/TestConfig.h"
)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/res/run.hcs.json.in"
        "${CMAKE_CURRENT_BINARY_DIR}/res/run.hcs.json"
)

# Move test shader folder
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res/shaders")
set(BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/res")
file(COPY "${SOURCE_DIR}" DESTINATION "${BINARY_DIR}")

# Set up test sources
file(GLOB_RECURSE DEMO_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(${DEMO_PROJECT_NAME} ${DEMO_CPP_FILES})
target_link_libraries(${DEMO_PROJECT_NAME}
        PRIVATE
        ${PROJECT_NAME}
)
target_include_directories(${DEMO_PROJECT_NAME}
        PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/include"
)
