# darknessrecomp - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(darknessrecomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(DARKNESSRECOMP_SOURCES
    src/main.cpp
    src/vr_viewport_probe.cpp
)

if(WIN32)
    add_executable(darknessrecomp WIN32 ${DARKNESSRECOMP_SOURCES})
else()
    add_executable(darknessrecomp ${DARKNESSRECOMP_SOURCES})
endif()

rexglue_setup_target(darknessrecomp)
