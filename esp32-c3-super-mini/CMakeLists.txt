cmake_minimum_required(VERSION 3.16)

# Silence a harmless warning from ESP-IDF's bootloader build on newer CMake
if(POLICY CMP0225)
    cmake_policy(SET CMP0225 OLD)
endif()

set(EXTRA_COMPONENT_DIRS src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(fadmr_c3)