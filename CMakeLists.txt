cmake_minimum_required(VERSION 3.16)

# Enable component manager
set(IDF_COMPONENT_MANAGER 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(RaceBox)

# Optional: mark extra components dir if we later add vendor drivers
# set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/components)


