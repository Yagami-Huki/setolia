cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# Both are required by SETOLIA (dock, frontend events); ON matches CMakePresets.json.
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(ENABLE_TESTS "Build OBS-independent unit tests (run with ctest)" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/setolia.cpp
    src/setolia.hpp
    src/dock/widget.cpp
    src/dock/widget.hpp
    src/dock/view.hpp
    src/dock/action.cpp
    src/dock/action.hpp
    src/source/template_manager.cpp
    src/source/template_manager.hpp
    src/source/template_paths.cpp
    src/source/template_paths.hpp
    src/source/template_paths_obs.cpp
    src/source/tagged_text_source_manager.cpp
    src/source/tagged_text_source_manager.hpp
    src/source/setolia_text_sources.cpp
    src/source/setolia_text_sources.hpp
    src/source/http_sync_server.cpp
    src/source/http_sync_server.hpp
    src/source/ws_frame.hpp
    src/source/timestamp.cpp
    src/source/timestamp.hpp
    src/source/next_request_policy.cpp
    src/source/next_request_policy.hpp
    src/config/config_manager.cpp
    src/config/config_manager.hpp
)

# Names of the bundled templates, used to find custom templates that older versions kept in the bundled folder.
file(
  GLOB _setolia_template_entries
  LIST_DIRECTORIES true
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/data/templates/*"
)
set(SETOLIA_BUILTIN_TEMPLATE_NAMES "")
foreach(_entry IN LISTS _setolia_template_entries)
  if(IS_DIRECTORY "${_entry}")
    cmake_path(GET _entry FILENAME _template_name)
    string(APPEND SETOLIA_BUILTIN_TEMPLATE_NAMES "\t\"${_template_name}\",\n")
  endif()
endforeach()
configure_file(src/source/builtin_templates.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/builtin_templates.hpp" @ONLY)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE PLUGIN_VERSION="${CMAKE_PROJECT_VERSION}")

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Plugin manifest read by the OBS 32+ plugin manager (<data>/manifest.json; ignored by older OBS).
# Generated so its version always matches buildspec.json.
string(JSON SETOLIA_MANIFEST_DISPLAY_NAME GET ${buildspec} displayName)
set(SETOLIA_MANIFEST_NAME "${_name}")
set(SETOLIA_MANIFEST_VERSION "${_version}")
configure_file(cmake/common/manifest.json.in "${CMAKE_CURRENT_BINARY_DIR}/manifest.json" @ONLY)
target_add_resource(${CMAKE_PROJECT_NAME} "${CMAKE_CURRENT_BINARY_DIR}/manifest.json")

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
