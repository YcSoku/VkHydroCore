cmake_minimum_required(VERSION 3.20)

set (PROJECT_NAME "HydroCorePlayer")
project(${PROJECT_NAME})
set(CMAKE_CXX_FLAGS_HIGH_PERFORMANCE "-Ofast -march=native -funroll-loops -flto -fomit-frame-pointer")

# Set variables
set(CMAKE_CXX_STANDARD 20)
set(ENABLE_VALIDATION_LAYER TRUE)
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/res")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SHADER_SOURCE_DIR "${RESOURCE_DIR}/shaders")
set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders")
set(INCLUDE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/include")
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
set(LOCAL_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party/local_lib")
set(EXTERNAL_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party/external_lib")
set(LOCAL_THIRD_PARTY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/third_party/local_lib")
set(EXTERNAL_THIRD_PARTY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/third_party/external_lib")

# hydroCore
add_subdirectory("${CMAKE_SOURCE_DIR}/libVkHydroCore")
list(APPEND LIBS VkHydroCore)

# Find cpp files
file(GLOB_RECURSE CPP_FILES ${SOURCE_DIR}/*.cpp)

add_executable(${PROJECT_NAME}
        ${CPP_FILES}
)
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        ${LIBS}
)
target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${INCLUDE_DIR}
        ${stb_SOURCE_DIR}
        ${INCLUDE_OUTPUT_DIR}
        ${Vulkan_INCLUDE_DIRS}
        ${NLOHMANN_JSON_INCLUDE_BUILD_DIR}
)
