# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-src")
  file(MAKE_DIRECTORY "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-src")
endif()
file(MAKE_DIRECTORY
  "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-build"
  "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-subbuild/osqp-populate-prefix"
  "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-subbuild/osqp-populate-prefix/tmp"
  "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-subbuild/osqp-populate-prefix/src/osqp-populate-stamp"
  "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-subbuild/osqp-populate-prefix/src"
  "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-subbuild/osqp-populate-prefix/src/osqp-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-subbuild/osqp-populate-prefix/src/osqp-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/storage/fin_proj/volatility_arbitrage_detection/volatility/build/_deps/osqp-subbuild/osqp-populate-prefix/src/osqp-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
