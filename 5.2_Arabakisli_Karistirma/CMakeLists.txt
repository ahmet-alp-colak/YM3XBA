cmake_minimum_required(VERSION 3.15)
project(CognitiveRF_LookThroughJamming VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# COMPILER SETTINGS
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags (mandatory for project compliance)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# ============================================================================
# DEPENDENCIES
# ============================================================================

# Threads (required for multi-threading)
find_package(Threads REQUIRED)

# Optional: libhackrf (for actual hardware control)
# find_package(LibHackRF)
# if(LibHackRF_FOUND)
#     add_definitions(-DHACKRF_AVAILABLE)
# endif()

# ============================================================================
# SOURCE FILES
# ============================================================================

set(HEADER_FILES
    include/DataStructures.hpp
    include/LookThroughEngine.hpp
    include/LookThroughPipeline.hpp
)

set(SOURCE_FILES
    src/LookThroughEngine.cpp
    src/LookThroughPipeline.cpp
)

# ============================================================================
# LIBRARY TARGET
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
# if(LibHackRF_FOUND)
#     target_link_libraries(${PROJECT_NAME} PRIVATE ${LibHackRF_LIBRARIES})
#     target_include_directories(${PROJECT_NAME} PRIVATE ${LibHackRF_INCLUDE_DIRS})
# endif()

# ============================================================================
# COMPILER-SPECIFIC FLAGS
# ============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -ffast-math
        -ftree-vectorize
    )
endif()

# ============================================================================
# EXAMPLE EXECUTABLE (OPTIONAL)
# ============================================================================

option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_executable(example_look_through examples/example_usage.cpp)
    target_link_libraries(example_look_through PRIVATE ${PROJECT_NAME})
    
    # Set output directory
    set_target_properties(example_look_through PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${HEADER_FILES}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CognitiveRF/LookThroughJamming
)

install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE CognitiveRF::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# ============================================================================
# SUMMARY
# ============================================================================

message(STATUS "")
message(STATUS "=============================================================")
message(STATUS "CognitiveRF Look-Through Jamming Module Configuration")
message(STATUS "=============================================================")
message(STATUS "  Version        : ${PROJECT_VERSION}")
message(STATUS "  Build type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard   : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler       : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build examples : ${BUILD_EXAMPLES}")
message(STATUS "=============================================================")
message(STATUS "")