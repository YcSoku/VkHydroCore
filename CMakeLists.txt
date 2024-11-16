cmake_minimum_required(VERSION 3.20)
include(FetchContent)

set (PROJECT_NAME "VkHydroCore")
project(${PROJECT_NAME})

# Set variables
set(CMAKE_CXX_STANDARD 17)
set(ENABLE_VALIDATION_LAYER FALSE)
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/test/res")
set(INCLUDE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/include")
set(CONDA_INCLUDE_DIR "${CONDA_ENVIRONMENT_PATH}/include")

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

set(GOOGLE_GIT "https://github.com/google")
set(KHRONOS_GIT "https://github.com/KhronosGroup")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ENABLE_VALIDATION_LAYER TRUE)
endif()

if (APPLE)
    set(PLATFORM_NEED_PORTABILITY TRUE)
endif ()

# Vulkan
find_package(Vulkan)
if (Vulkan_FOUND)
    message(STATUS "Found Vulkan SDK at: ${Vulkan_LIBRARIES}")
else()
    message(FATAL_ERROR "Error: Unable to locate Vulkan SDK. Please install the Vulkan SDK.")
endif()
list(APPEND LIBS ${Vulkan_LIBRARIES})

# Compile shaders
#set(SHADER_SOURCE_DIR "${RESOURCE_DIR}/shaders")
#set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders")
#set(SPIRV_FILES)
#file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})
#file(GLOB SHADER_FILES
#        "${SHADER_SOURCE_DIR}/*.vert"
#        "${SHADER_SOURCE_DIR}/*.frag"
#        "${SHADER_SOURCE_DIR}/*.comp"
#)
#foreach (SHADER_FILE ${SHADER_FILES})
#    get_filename_component(FILENAME ${SHADER_FILE} NAME)
#    set(SPIRV_FILE "${SHADER_BINARY_DIR}/${FILENAME}.spv")
#
#    add_custom_command(
#            OUTPUT ${SPIRV_FILE}
#            COMMAND "${Vulkan_GLSLC_EXECUTABLE}" ${SHADER_FILE} -o ${SPIRV_FILE}
#            DEPENDS ${SHADER_FILE}
#            COMMENT "Compiling ${FILENAME} to SPIR-V"
#    )
#    list(APPEND SPIRV_FILES ${SPIRV_FILE})
#endforeach ()
#add_custom_target(CompileShaders
#        DEPENDS ${SPIRV_FILES}
#)
#add_dependencies(${PROJECT_NAME} CompileShaders)

# Configure header file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in
        ${INCLUDE_OUTPUT_DIR}/config.h)

# spirv-reflect
OPTION(SPIRV_REFLECT_EXECUTABLE     "Build spirv-reflect executable" OFF)
OPTION(SPIRV_REFLECT_STATIC_LIB     "Build a SPIRV-Reflect static library" ON)
FetchContent_Declare(
        SPIRV-Reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG main
)
FetchContent_MakeAvailable(SPIRV-Reflect)
list(APPEND LIBS spirv-reflect-static)

# spirv-tools
FetchContent_Declare(
        spirv-tools
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
        GIT_TAG 44d72a9b36702f093dd20815561a56778b2d181e
)
FetchContent_Declare(
        spirv-headers
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
        GIT_TAG 1feaf4414eb2b353764d01d88f8aa4bcc67b60db
)

#glslang
FetchContent_Declare(
        glslang
        GIT_REPOSITORY "${KHRONOS_GIT}/glslang.git"
        GIT_TAG ef77cf3a92490f7c37f36f20263cd3cd8c94f009
)
FetchContent_MakeAvailable(spirv-headers spirv-tools glslang)

# shaderc
option(SHADERC_SKIP_TESTS "Skip shaderc tests" ON)
FetchContent_Declare(
        shaderc
        GIT_REPOSITORY https://github.com/google/shaderc.git
        GIT_TAG v2023.3
)
FetchContent_MakeAvailable(shaderc)
list(APPEND LIBS shaderc)

# glm
FetchContent_Declare(
        glm
        GIT_REPOSITORY	https://github.com/g-truc/glm.git
        GIT_TAG 	    bf71a834948186f4097caa076cd2663c69a10e1e #refs/tags/1.0.1
)
FetchContent_MakeAvailable(glm)
list(APPEND LIBS glm::glm)

# stb
FetchContent_Declare(
        stb
        GIT_REPOSITORY  https://github.com/nothings/stb.git
        GIT_TAG 	    2e2bef463a5b53ddf8bb788e25da6b8506314c08
)
FetchContent_MakeAvailable(stb)

# json
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY  https://github.com/nlohmann/json.git
        GIT_TAG 	    63258397761b3dd96dd171e5a5ad5aa915834c35
)
FetchContent_MakeAvailable(nlohmann_json)
list(APPEND LIBS nlohmann_json::nlohmann_json)

# Pybind11
FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)
list(APPEND LIBS pybind11::module)

# RTTR
find_package(RTTR CONFIG REQUIRED Core)
list(APPEND LIBS RTTR::Core)

# Find cpp files
file(GLOB_RECURSE CPP_FILES ${SOURCE_DIR}/*.cpp)

add_library(${PROJECT_NAME}
        SHARED
        ${CPP_FILES}
)
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        ${LIBS}
)
target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${INCLUDE_DIR}
        ${INCLUDE_OUTPUT_DIR}
        ${Vulkan_INCLUDE_DIRS}
)

# -- Add pybind11 module --
set(PYTHON_MODULE_NAME "pyHydroCore")
file(GLOB_RECURSE PYBIND_FILES ${CMAKE_SOURCE_DIR}/pybinding/*.cpp)
pybind11_add_module(${PYTHON_MODULE_NAME} ${PYBIND_FILES})

message(STATUS "Pybind11 include directory: ${pybind11_INCLUDE_DIR}")
target_link_libraries(${PYTHON_MODULE_NAME}
        PUBLIC
        pybind11::module
        ${PROJECT_NAME}
)

# -- Add test target --
# Set up test sources
file(GLOB_RECURSE TEST_FILES ${CMAKE_SOURCE_DIR}/test/src/*.cpp)
message(STATUS "Test files: ${TEST_FILES}")

# Create the test executable
set(TEST_PROJECT_NAME "${PROJECT_NAME}Demo")
add_executable(${TEST_PROJECT_NAME} ${TEST_FILES})

# Link the test executable with the necessary libraries
target_link_libraries(${TEST_PROJECT_NAME}
        PRIVATE
        ${PROJECT_NAME}
)
target_include_directories(${TEST_PROJECT_NAME}
        PRIVATE
        ${INCLUDE_DIR}
        ${INCLUDE_OUTPUT_DIR}
        ${Vulkan_INCLUDE_DIRS}
)
