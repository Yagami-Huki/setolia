# OBS-independent unit tests. Configure with -DENABLE_TESTS=ON and run ctest.

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
