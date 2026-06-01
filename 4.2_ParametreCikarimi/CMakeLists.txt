cmake_minimum_required(VERSION 3.15)
project(CognitiveRF_ParameterExtraction VERSION 1.0.0 LANGUAGES CXX)

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

# Threads (required for std::thread)
find_package(Threads REQUIRED)

# ONNX Runtime (required for AI inference)
# Set ONNXRUNTIME_ROOT to your ONNX Runtime installation path
set(ONNXRUNTIME_ROOT "/usr/local" CACHE PATH "ONNX Runtime installation directory")
find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS ${ONNXRUNTIME_ROOT}/include
)
find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS ${ONNXRUNTIME_ROOT}/lib
)

if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
    message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIBRARY}")
    set(ONNXRUNTIME_FOUND TRUE)
else()
    message(WARNING "ONNX Runtime not found. AI classifier will be disabled.")
    set(ONNXRUNTIME_FOUND FALSE)
endif()

# SQLite3 (required for persistence)
find_package(SQLite3 REQUIRED)

# Optional: FFTW3 (for production DSP - currently using simplified version)
find_package(FFTW3 QUIET)
if(FFTW3_FOUND)
    message(STATUS "Found FFTW3: ${FFTW3_LIBRARIES}")
    add_definitions(-DUSE_FFTW3)
endif()

# Optional: liquid-dsp (for production demodulation)
find_library(LIQUID_DSP_LIBRARY
    NAMES liquid
    PATHS /usr/local/lib /usr/lib
)
if(LIQUID_DSP_LIBRARY)
    message(STATUS "Found liquid-dsp: ${LIQUID_DSP_LIBRARY}")
    add_definitions(-DUSE_LIQUID_DSP)
endif()

# ============================================================================
# Source Files
# ============================================================================

set(HEADER_FILES
    include/DataStructures.hpp
    include/KalmanCFOTracker.hpp
    include/SignalCorrector.hpp
    include/ModulationClassifier.hpp
    include/ProtocolAnalyzer.hpp
    include/DSPEngine.hpp
    include/ParameterExtractionPipeline.hpp
    include/PersistenceLogger.hpp
)

set(SOURCE_FILES
    src/KalmanCFOTracker.cpp
    src/SignalCorrector.cpp
    src/ModulationClassifier.cpp
    src/ProtocolAnalyzer.cpp
    src/DSPEngine.cpp
    src/ParameterExtractionPipeline.cpp
    src/PersistenceLogger.cpp
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
        SQLite::SQLite3
)

# Conditional ONNX Runtime linking
if(ONNXRUNTIME_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${ONNXRUNTIME_LIBRARY})
    target_compile_definitions(${PROJECT_NAME} PUBLIC ONNXRUNTIME_AVAILABLE)
endif()

# Optional FFTW3
if(FFTW3_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARIES})
endif()

# Optional liquid-dsp
if(LIQUID_DSP_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIQUID_DSP_LIBRARY})
endif()

# ============================================================================
# Example/Test Executable (Optional)
# ============================================================================

option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_executable(example_usage examples/example_usage.cpp)
    target_link_libraries(example_usage PRIVATE ${PROJECT_NAME})
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
# Package Configuration
# ============================================================================

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
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
message(STATUS "  ONNX Runtime:    ${ONNXRUNTIME_FOUND}")
message(STATUS "  SQLite3:         ${SQLite3_FOUND}")
message(STATUS "  FFTW3:           ${FFTW3_FOUND}")
message(STATUS "  liquid-dsp:      ${LIQUID_DSP_LIBRARY}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Build examples:  ${BUILD_EXAMPLES}")
message(STATUS "========================================")
message(STATUS "")
