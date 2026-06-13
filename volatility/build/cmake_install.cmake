# Install script for directory: C:/storage/fin_proj/volatility_arbitrage_detection/volatility

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/vol_arb")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/json-build/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/Debug/vol_arb.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/Release/vol_arb.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/MinSizeRel/vol_arb.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/RelWithDebInfo/vol_arb.exe")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/vol_arb" TYPE FILE FILES
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/vol_surface.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/arbitrage_detector.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/qp_solver.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/dual_certificate.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/local_vol.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/svi_surface.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/data_handler.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/logger.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/config_manager.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/test_framework.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/vol_api.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/memory_pool.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/optimization_hints.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/simd_math.hpp"
    "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/include/validation.hpp"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
