# Set minimum required CMake version
cmake_minimum_required(VERSION 3.5)

# Set project name
project(mti_example)

# Set C++ standard to C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include ExternalProject module
include(ExternalProject)

# Define the external project for xspublic
ExternalProject_Add(
    xspublic
    PREFIX ${CMAKE_BINARY_DIR}/xspublic
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/xspublic
    CONFIGURE_COMMAND ""  # No configure step
    BUILD_COMMAND make
    BUILD_IN_SOURCE 1  # Build in the source directory
    INSTALL_COMMAND ""  # No install step
)

add_custom_target(clean_all
    COMMAND make clean
    COMMAND cd ${CMAKE_SOURCE_DIR}/xspublic && make clean
)

# Find project source files
file(GLOB_RECURSE PROJECT_SOURCES 
    "src/*.cpp"
    "main.cpp"
)

# Find project header files
file(GLOB_RECURSE PROJECT_HEADERS 
    "include/*.h"
)

# Specify additional locations of header files
include_directories(
    ${CMAKE_SOURCE_DIR}/xspublic
    ${CMAKE_SOURCE_DIR}/include
)

# Link directories for the libraries
link_directories(
    ${CMAKE_SOURCE_DIR}/xspublic/xscontroller
    ${CMAKE_SOURCE_DIR}/xspublic/xscommon
    ${CMAKE_SOURCE_DIR}/xspublic/xstypes
)

# Add the executable
add_executable(
    mti_example
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

# Make sure mti_example depends on xspublic build
add_dependencies(mti_example xspublic)

# Link the required libraries
target_link_libraries(
    mti_example
    xscontroller
    xscommon
    xstypes
    pthread
    dl
)