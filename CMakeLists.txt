cmake_minimum_required(VERSION 3.18)

project(TMR LANGUAGES CXX)

# ---- Options ----
option(TMR_BUILD_PYTHON "Build the pybind11 Python module" ON)
option(BUILD_SHARED_LIBS "Build shared libs" ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---- MPI ----
find_package(MPI REQUIRED)

# ---- Core library ----
file(GLOB_RECURSE TMR_CORE_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(tmr ${TMR_CORE_SOURCES})

target_include_directories(tmr
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tmr
  PUBLIC
    MPI::MPI_CXX
)

if(TMR_BUILD_PYTHON)
  find_package(pybind11 CONFIG REQUIRED)

  # Build an extension module whose import name is "tmr.TMR"
  pybind11_add_module(TMR MODULE
    ${CMAKE_CURRENT_SOURCE_DIR}/tmr/TMR.cpp
  )

  target_link_libraries(TMR
    PRIVATE
      tmr
      MPI::MPI_CXX
  )

  target_include_directories(TMR
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  # Put the built extension into the python package directory for in-tree usage:
  set_target_properties(TMR PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tmr
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tmr
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tmr
  )
endif()
