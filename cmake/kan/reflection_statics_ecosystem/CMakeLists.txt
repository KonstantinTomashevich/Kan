# Ecosystem CMake script for reflection statics from testing.cmake in parent directory.

file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated/${ARTEFACT_NAME})
set (REFLECTION_STATICS_FILE "${CMAKE_BINARY_DIR}/generated/${ARTEFACT_NAME}/reflection_statics.c")
message (STATUS "    Save reflection statics as \"${REFLECTION_STATICS_FILE}\".")
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/reflection_statics.c" "${REFLECTION_STATICS_FILE}")

register_concrete ("${ARTEFACT_NAME}_reflection_statics")
concrete_sources_direct ("${REFLECTION_STATICS_FILE}")
concrete_require (SCOPE PRIVATE ABSTRACT reflection)
shared_library_include (SCOPE PUBLIC CONCRETE "${ARTEFACT_NAME}_reflection_statics")
