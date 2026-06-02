#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "CognitiveRF::CognitiveRF_DirectionFinding" for configuration "Release"
set_property(TARGET CognitiveRF::CognitiveRF_DirectionFinding APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(CognitiveRF::CognitiveRF_DirectionFinding PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libCognitiveRF_DirectionFinding.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS CognitiveRF::CognitiveRF_DirectionFinding )
list(APPEND _IMPORT_CHECK_FILES_FOR_CognitiveRF::CognitiveRF_DirectionFinding "${_IMPORT_PREFIX}/lib/libCognitiveRF_DirectionFinding.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
