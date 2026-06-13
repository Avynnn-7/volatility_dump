#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "osqp::osqp" for configuration "MinSizeRel"
set_property(TARGET osqp::osqp APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(osqp::osqp PROPERTIES
  IMPORTED_IMPLIB_MINSIZEREL "${_IMPORT_PREFIX}/lib/osqp.lib"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/bin/osqp.dll"
  )

list(APPEND _cmake_import_check_targets osqp::osqp )
list(APPEND _cmake_import_check_files_for_osqp::osqp "${_IMPORT_PREFIX}/lib/osqp.lib" "${_IMPORT_PREFIX}/bin/osqp.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
