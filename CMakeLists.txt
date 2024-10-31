cmake_minimum_required(VERSION 3.20)
include(FetchContent)

set (PROJECT_NAME "VkHydroCore")
project(${PROJECT_NAME})

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

# Vulkan
find_package(Vulkan)
if (Vulkan_FOUND)
    message(STATUS "Found Vulkan SDK at: ${Vulkan_LIBRARIES}")
else()
    message(FATAL_ERROR "Error: Unable to locate Vulkan SDK. Please install the Vulkan SDK.")
endif()
list(APPEND LIBS ${Vulkan_LIBRARIES})

# Compile shaders
set(SPIRV_FILES)
file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})
file(GLOB SHADER_FILES
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
)
foreach (SHADER_FILE ${SHADER_FILES})
    get_filename_component(FILENAME ${SHADER_FILE} NAME)
    set(SPIRV_FILE "${SHADER_BINARY_DIR}/${FILENAME}.spv")

    add_custom_command(
            OUTPUT ${SPIRV_FILE}
            COMMAND "${Vulkan_GLSLC_EXECUTABLE}" ${SHADER_FILE} -o ${SPIRV_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling ${FILENAME} to SPIR-V"
    )
    list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach ()
add_custom_target(CompileShaders
        DEPENDS ${SPIRV_FILES}
)

# Configure header file
configure_file(${CMAKE_SOURCE_DIR}/include/config.h.in
        ${INCLUDE_OUTPUT_DIR}/config.h)

# glm
FetchContent_Declare(
        glm
        GIT_REPOSITORY	https://github.com/g-truc/glm.git
        GIT_TAG 	bf71a834948186f4097caa076cd2663c69a10e1e #refs/tags/1.0.1
)
FetchContent_MakeAvailable(glm)
list(APPEND LIBS glm::glm)

# stb
#set(STB_DIR "${LOCAL_THIRD_PARTY_DIR}/stb")
#include_directories("${STB_DIR}/include")
#add_library(stb "${STB_DIR}/stb_image.cpp")
#list(APPEND LIBS stb)
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG 	2e2bef463a5b53ddf8bb788e25da6b8506314c08
)
FetchContent_MakeAvailable(stb)

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
)
add_dependencies(${PROJECT_NAME} CompileShaders)
