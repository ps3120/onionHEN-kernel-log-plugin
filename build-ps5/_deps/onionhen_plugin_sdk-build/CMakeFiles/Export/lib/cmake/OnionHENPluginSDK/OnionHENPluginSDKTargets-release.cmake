#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OnionHEN::Runtime" for configuration "Release"
set_property(TARGET OnionHEN::Runtime APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OnionHEN::Runtime PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libonion_sdk_runtime.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS OnionHEN::Runtime )
list(APPEND _IMPORT_CHECK_FILES_FOR_OnionHEN::Runtime "${_IMPORT_PREFIX}/lib/libonion_sdk_runtime.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
