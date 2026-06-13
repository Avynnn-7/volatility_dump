
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was osqp-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

####################################################################################

# CMake 2.6.4 and before didn't support ON in if() statements, so to ensure compatibility
# add some temp variables indicating the build options.
SET( OSQP_HAVE_SHARED_LIB ON )
SET( OSQP_HAVE_STATIC_LIB ON )

if( ${OSQP_HAVE_SHARED_LIB} )
    include( "${CMAKE_CURRENT_LIST_DIR}/osqp-targets.cmake" )
endif()

if( ${OSQP_HAVE_STATIC_LIB} )
    include( "${CMAKE_CURRENT_LIST_DIR}/osqpstatic-targets.cmake" )
endif()
