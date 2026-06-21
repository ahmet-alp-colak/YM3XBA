cmake_minimum_required(VERSION 3.15)
project(CognitiveRF_SignalDetection VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# C++ STANDARD (C++17 - MANDATORY)
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# COMPILER FLAGS
# ============================================================================
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# ============================================================================
# DEPENDENCIES
# ============================================================================

# Threads (REQUIRED)
find_package(Threads REQUIRED)

# FFTW3 (REQUIRED for Welch PSD)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFTW3 fftw3)
endif()

if(NOT FFTW3_FOUND)
    # Try to find FFTW3 manually
    find_path(FFTW3_INCLUDE_DIR fftw3.h
        PATHS /usr/include /usr/local/include
    )
    find_library(FFTW3_LIBRARY fftw3
        PATHS /usr/lib /usr/local/lib
    )
    
    if(FFTW3_INCLUDE_DIR AND FFTW3_LIBRARY)
        set(FFTW3_FOUND TRUE)
        set(FFTW3_INCLUDE_DIRS ${FFTW3_INCLUDE_DIR})
        set(FFTW3_LIBRARIES ${FFTW3_LIBRARY})
        message(STATUS "Found FFTW3: ${FFTW3_LIBRARY}")
    else()
        message(WARNING "FFTW3 not found. Build may fail.")
    endif()
endif()

# ============================================================================
# SOURCE FILES
# ============================================================================
set(HEADER_FILES
    include/DataStructures.hpp
    include/ThreadSafeQueue.hpp
    include/WelchPSDProcessor.hpp
    include/FeatureExtractor.hpp
    include/GMMClassifier.hpp
    include/TemporalStateEngine.hpp
    include/SignalDetectionEngine.hpp
    include/SignalDetectionPipeline.hpp
)

set(SOURCE_FILES
    src/DataStructures.cpp
    src/WelchPSDProcessor.cpp
    src/FeatureExtractor.cpp
    src/GMMClassifier.cpp
    src/TemporalStateEngine.cpp
    src/SignalDetectionEngine.cpp
    src/SignalDetectionPipeline.cpp
)

# ============================================================================
# LIBRARY TARGET
# ============================================================================
add_library(${PROJECT_NAME} STATIC ${HEADER_FILES} ${SOURCE_FILES})

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Threads::Threads
)

# Link FFTW3 if found
if(FFTW3_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${FFTW3_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARIES})
endif()

# ============================================================================
# EXAMPLE EXECUTABLE
# ============================================================================
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_executable(example_signal_detection examples/example_usage.cpp)
    target_link_libraries(example_signal_detection PRIVATE ${PROJECT_NAME})
    
    # Set output directory
    set_target_properties(example_signal_detection
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# ============================================================================
# INSTALLATION
# ============================================================================
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${HEADER_FILES}
    DESTINATION include/CognitiveRF/SignalDetection
)

# ============================================================================
# PACKAGE INFORMATION
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "CognitiveRF Signal Detection Module")
message(STATUS "========================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "FFTW3: ${FFTW3_FOUND}")
message(STATUS "Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "========================================")
message(STATUS "")
