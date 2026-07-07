cmake_minimum_required(VERSION 3.15)
project(CognitiveRF_SpotJamming VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# C++ Standard Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Compiler Flags
# ============================================================================
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")

# ============================================================================
# Dependencies
# ============================================================================
find_package(Threads REQUIRED)

# Optional: libhackrf (for hardware TX control)
find_library(HACKRF_LIBRARY hackrf)
if(HACKRF_LIBRARY)
    message(STATUS "libhackrf found: ${HACKRF_LIBRARY}")
    set(HACKRF_AVAILABLE ON)
else()
    message(STATUS "libhackrf NOT found - using placeholder TX functions")
    set(HACKRF_AVAILABLE OFF)
endif()

# ============================================================================
# Source Files
# ============================================================================
set(HEADER_FILES
    include/DataStructures.hpp
    include/SpotJammingEngine.hpp
    include/SpotJammingPipeline.hpp
)

set(SOURCE_FILES
    src/SpotJammingEngine.cpp
    src/SpotJammingPipeline.cpp
)

# ============================================================================
# Library Target
# ============================================================================
add_library(${PROJECT_NAME} STATIC ${HEADER_FILES} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Threads::Threads
)

# Optional HackRF linking
if(HACKRF_AVAILABLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${HACKRF_LIBRARY})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HACKRF_AVAILABLE)
endif()

# ============================================================================
# Example Executable
# ============================================================================
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_executable(example_usage examples/example_usage.cpp)
    target_link_libraries(example_usage PRIVATE ${PROJECT_NAME})
    
    message(STATUS "Example executable will be built: example_usage")
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${HEADER_FILES}
    DESTINATION include/CognitiveRF/SpotJamming
)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "=====================================================")
message(STATUS "CognitiveRF Spot Jamming Module Configuration")
message(STATUS "=====================================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "HackRF Support: ${HACKRF_AVAILABLE}")
message(STATUS "=====================================================")
message(STATUS "")
