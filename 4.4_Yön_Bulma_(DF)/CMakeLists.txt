cmake_minimum_required(VERSION 3.15)
project(CognitiveRF_DirectionFinding VERSION 1.0.0 LANGUAGES CXX)

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

# Optional: liquid-dsp (for advanced demodulation)
find_library(LIQUID_DSP_LIBRARY
    NAMES liquid
    PATHS /usr/local/lib /usr/lib
)
if(LIQUID_DSP_LIBRARY)
    message(STATUS "Found liquid-dsp: ${LIQUID_DSP_LIBRARY}")
    add_definitions(-DUSE_LIQUID_DSP)
else()
    message(WARNING "liquid-dsp not found. Using simplified demodulation.")
endif()

# Optional: FFTW3 (for future FFT-based improvements)
find_package(FFTW3 QUIET)
if(FFTW3_FOUND)
    message(STATUS "Found FFTW3: ${FFTW3_LIBRARIES}")
    add_definitions(-DUSE_FFTW3)
else()
    message(WARNING "FFTW3 not found.")
endif()

# ============================================================================
# Source Files
# ============================================================================

set(HEADER_FILES
    include/DataStructures.hpp
    include/DirectionFindingEngine.hpp
    include/AntennaSwitcher.hpp
    include/PseudoDopplerDemodulator.hpp
    include/PhaseComparator.hpp
    include/DirectionFindingPipeline.hpp
)

set(SOURCE_FILES
    src/DirectionFindingEngine.cpp
    src/AntennaSwitcher.cpp
    src/PseudoDopplerDemodulator.cpp
    src/PhaseComparator.cpp
    src/DirectionFindingPipeline.cpp
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

# Optional liquid-dsp
if(LIQUID_DSP_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIQUID_DSP_LIBRARY})
endif()

# Optional FFTW3
if(FFTW3_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARIES})
endif()

# ============================================================================
# Example/Test Executable (Optional)
# ============================================================================

option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_executable(example_direction_finding examples/example_usage.cpp)
    target_link_libraries(example_direction_finding PRIVATE ${PROJECT_NAME})
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
message(STATUS "  liquid-dsp:      ${LIQUID_DSP_LIBRARY}")
message(STATUS "  FFTW3:           ${FFTW3_FOUND}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Build examples:  ${BUILD_EXAMPLES}")
message(STATUS "========================================")
message(STATUS "")
