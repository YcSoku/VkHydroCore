cmake_minimum_required(VERSION 3.20)

FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# Configure test python file
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test/test.py.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test.py"
)

set(PYTHON_MODULE_NAME "pyHydroCore")
file(GLOB_RECURSE PYBIND_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
pybind11_add_module(${PYTHON_MODULE_NAME} MODULE ${PYBIND_FILES})

target_link_libraries(${PYTHON_MODULE_NAME}
        PRIVATE
        ${PROJECT_NAME}
        pybind11::module
)
set_target_properties(${PYTHON_MODULE_NAME}
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
