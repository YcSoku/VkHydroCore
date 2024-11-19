cmake_minimum_required(VERSION 3.20)
include(FetchContent)

set (PY_PROJECT_NAME "pyHydroCore")

FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)
list(APPEND LIBS pybind11::module)

set(PYTHON_MODULE_NAME "pyHydroCore")
file(GLOB_RECURSE PYBIND_FILES ${CMAKE_SOURCE_DIR}/python/*.cpp)
pybind11_add_module(${PYTHON_MODULE_NAME} MODULE ${PYBIND_FILES})

target_link_libraries(${PYTHON_MODULE_NAME}
        PRIVATE
        pybind11::module
        ${PROJECT_NAME}
)
set_target_properties(${PYTHON_MODULE_NAME}
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python"
)
