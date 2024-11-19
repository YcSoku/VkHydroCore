cmake_minimum_required(VERSION 3.20)

set (DEMO_PROJECT_NAME "VkHydroCoreDemo")
set (TEST_RESOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/res")

# Configure resource file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/res/run.hcs.org.json.in
        ${CMAKE_CURRENT_SOURCE_DIR}/res/run.hcs.org.json)

# Set up test sources
file(GLOB_RECURSE DEMO_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(${DEMO_PROJECT_NAME} ${DEMO_CPP_FILES})
target_link_libraries(${DEMO_PROJECT_NAME}
        PRIVATE
        ${PROJECT_NAME}
)
target_include_directories(${DEMO_PROJECT_NAME}
        PRIVATE
        ${INCLUDE_DIR}
        ${INCLUDE_OUTPUT_DIR}
        ${Vulkan_INCLUDE_DIRS}
)
