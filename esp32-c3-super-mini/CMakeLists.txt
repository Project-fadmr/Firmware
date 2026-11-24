cmake_minimum_required(VERSION 3.22)

# our code lives in src/ instead of IDF's default main/
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fadmr_c3)
