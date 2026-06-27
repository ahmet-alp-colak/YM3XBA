cmake_minimum_required(VERSION 3.15)
project(CognitiveRF_Geolocation VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# Project Configuration
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# ============================================================================
# Dependencies
# ============================================================================

# Threads (required for pipeline)
find_package(Threads REQUIRED)

# Optional: Eigen3 (for advanced matrix operations)
find_package(Eigen3 QUIET)
if(EIGEN3_FOUND)
    message(STATUS "Found Eigen3: ${EIGEN3_INCLUDE_DIRS}")
    add_definitions(-DUSE_EIGEN3)
else()
    message(WARNING "Eigen3 not found. Using simplified matrix operations.")
endif()

# ============================================================================
# Source Files
# ============================================================================

set(HEADER_FILES
    include/DataStructures.hpp
    include/GeodeticUtils.hpp
    include/LineOfBearingCalculator.hpp
    include/CrossFixingSolver.hpp
    include/LeastSquaresOptimizer.hpp
    include/GeolocationEngine.hpp
    include/GeolocationPipeline.hpp
)

set(SOURCE_FILES
    src/GeodeticUtils.cpp
    src/LineOfBearingCalculator.cpp
    src/CrossFixingSolver.cpp
    src/LeastSquaresOptimizer.cpp
    src/GeolocationEngine.cpp
    src/GeolocationPipeline.cpp
)

# ============================================================================
# Library Target
# ============================================================================

add_library(${PROJECT_NAME} STATIC
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Threads::Threads
)

# Optional Eigen3
if(EIGEN3_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${EIGEN3_INCLUDE_DIRS})
endif()

# ============================================================================
# Example/Test Executable (Optional)
# ============================================================================

option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_executable(example_geolocation examples/example_usage.cpp)
    target_link_libraries(example_geolocation PRIVATE ${PROJECT_NAME})
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)

install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE CognitiveRF::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  Threads:         ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "  Eigen3:          ${EIGEN3_FOUND}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Build examples:  ${BUILD_EXAMPLES}")
message(STATUS "========================================")
message(STATUS "")
