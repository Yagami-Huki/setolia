# CMake Windows defaults module

include_guard(GLOBAL)

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

include(buildspec)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  file(TO_CMAKE_PATH "$ENV{ALLUSERSPROFILE}" ALLUSERSPROFILE_CMAKE)
  set(
    CMAKE_INSTALL_PREFIX
    "${ALLUSERSPROFILE_CMAKE}/obs-studio/plugins/${CMAKE_PROJECT_NAME}"
    CACHE PATH
    "Default plugin installation directory"
    FORCE
  )
endif()
