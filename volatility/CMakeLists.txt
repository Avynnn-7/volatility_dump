cmake_minimum_required(VERSION 3.20)
project(vol_arb VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(Eigen3 CONFIG QUIET)
if(NOT Eigen3_FOUND)
    FetchContent_Declare(eigen GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git GIT_TAG 3.4.0)
    FetchContent_MakeAvailable(eigen)
endif()

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(json)
endif()

include(FetchContent)
FetchContent_Declare(
    osqp
    GIT_REPOSITORY https://github.com/osqp/osqp.git
    GIT_TAG        v1.0.0.beta1
)
FetchContent_MakeAvailable(osqp)
if(TARGET osqpstatic)
    add_library(osqp::osqp ALIAS osqpstatic)
endif()

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found - parallelization enabled")
    set(VOL_ARB_OPENMP_ENABLED TRUE)
else()
    message(WARNING "OpenMP not found - parallelization disabled")
    set(VOL_ARB_OPENMP_ENABLED FALSE)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(LIB_SOURCES
    src/vol_surface.cpp
    src/arbitrage_detector.cpp
    src/qp_solver.cpp
    src/dual_certificate.cpp
    src/local_vol.cpp
    src/svi_surface.cpp
    src/data_handler.cpp
    src/logger.cpp
    src/config_manager.cpp
    src/vol_api.cpp
    src/profit_advisor.cpp
    src/upstox_client.cpp
    src/raw_arbitrage_scanner.cpp
)

include_directories(include)

add_executable(vol_arb main.cpp ${LIB_SOURCES})
target_include_directories(vol_arb PRIVATE include)
target_link_libraries(vol_arb PRIVATE
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    osqp::osqp
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(vol_arb PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(vol_arb PRIVATE VOL_ARB_OPENMP_ENABLED)
endif()

add_executable(vol_api_server src/api_server_main.cpp ${LIB_SOURCES})
target_include_directories(vol_api_server PRIVATE include)
target_link_libraries(vol_api_server PRIVATE
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    osqp::osqp
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(vol_api_server PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(vol_api_server PRIVATE VOL_ARB_OPENMP_ENABLED)
endif()

add_executable(vol_benchmark src/benchmark_main.cpp ${LIB_SOURCES})
target_include_directories(vol_benchmark PRIVATE include)
target_link_libraries(vol_benchmark PRIVATE
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    osqp::osqp
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(vol_benchmark PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(vol_benchmark PRIVATE VOL_ARB_OPENMP_ENABLED)
endif()

install(TARGETS vol_arb
    RUNTIME DESTINATION bin
)

install(FILES include/vol_surface.hpp
              include/arbitrage_detector.hpp
              include/qp_solver.hpp
              include/dual_certificate.hpp
              include/local_vol.hpp
              include/svi_surface.hpp
              include/data_handler.hpp
              include/logger.hpp
              include/config_manager.hpp
              include/test_framework.hpp
              include/vol_api.hpp
              include/memory_pool.hpp
              include/optimization_hints.hpp
              include/simd_math.hpp
              include/validation.hpp
    DESTINATION include/vol_arb
)

enable_testing()

set(TEST_SOURCES
    tests/test_main.cpp
    tests/unit/test_black_scholes.cpp
    tests/unit/test_arbitrage_detector.cpp
    tests/unit/test_qp_solver.cpp
    tests/edge_cases/test_edge_cases.cpp
    tests/stress/test_stress.cpp
    tests/performance/test_benchmarks.cpp
    tests/thread_safety/test_thread_safety.cpp
    tests/regression/test_regression.cpp
    tests/integration/test_integration.cpp
)

add_executable(vol_tests ${TEST_SOURCES} ${LIB_SOURCES})
target_include_directories(vol_tests PRIVATE include)
target_link_libraries(vol_tests PRIVATE
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    osqp::osqp
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(vol_tests PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(vol_tests PRIVATE VOL_ARB_OPENMP_ENABLED)
endif()

add_test(NAME AllTests COMMAND vol_tests)
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found - documentation target enabled")
    
    set(DOXYGEN_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
    
   
    file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})
    
    
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        @ONLY
    )
    
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen..."
        VERBATIM
    )
    
    install(DIRECTORY ${DOXYGEN_OUTPUT_DIR}/html
        DESTINATION share/doc/vol_arb
        OPTIONAL
    )
else()
    message(STATUS "Doxygen not found - documentation target disabled")
endif()

