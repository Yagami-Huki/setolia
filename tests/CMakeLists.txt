# OBS-independent unit tests. They only need Qt6 Core.
#  - With the plugin:   configure the project with -DENABLE_TESTS=ON and run ctest.
#  - Standalone (CI):   cmake -S tests -B build_tests && cmake --build build_tests && ctest --test-dir build_tests

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.28...3.30)
  project(setolia_tests LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
  enable_testing()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(ws_frame_test ws_frame_test.cpp test_support.hpp ../src/source/ws_frame.hpp)
add_test(NAME ws_frame_test COMMAND ws_frame_test)

add_executable(
  template_paths_test
  template_paths_test.cpp
  test_support.hpp
  ../src/source/template_paths.cpp
  ../src/source/template_paths.hpp
)
target_link_libraries(template_paths_test PRIVATE Qt6::Core)
add_test(NAME template_paths_test COMMAND template_paths_test)

add_executable(
  timestamp_test
  timestamp_test.cpp
  test_support.hpp
  ../src/source/timestamp.cpp
  ../src/source/timestamp.hpp
)
target_link_libraries(timestamp_test PRIVATE Qt6::Core)
add_test(NAME timestamp_test COMMAND timestamp_test)

add_executable(
  next_request_policy_test
  next_request_policy_test.cpp
  test_support.hpp
  ../src/source/next_request_policy.cpp
  ../src/source/next_request_policy.hpp
)
add_test(NAME next_request_policy_test COMMAND next_request_policy_test)
