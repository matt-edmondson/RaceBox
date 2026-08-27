# Host-side unit tests for the parts of the firmware that carry no ESP-IDF
# dependency. Build and run with:
#
#   cmake -S test/host -B build-host && cmake --build build-host
#   ./build-host/racebox_host_tests
#
cmake_minimum_required(VERSION 3.16)
project(racebox_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(racebox_host_tests
  RunTests.cpp
  TestUbxParser.cpp
  TestLapTimer.cpp
  TestMenuAndSettings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../main/ble/UbxParser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../main/ui/LapTimer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../main/ui/Menu.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../main/config/Settings.cpp
)

target_compile_options(racebox_host_tests PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME host_tests COMMAND racebox_host_tests)
