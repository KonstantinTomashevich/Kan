# Ecosystem CMake script for context statics from testing.cmake in parent directory.

set (CONTEXT_STATICS_FILE "${CMAKE_BINARY_DIR}/generated/${ARTEFACT_NAME}/context_statics.c")
message (STATUS "    Save context statics as \"${CONTEXT_STATICS_FILE}\".")
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/context_statics.c" "${CONTEXT_STATICS_FILE}")

register_concrete ("${ARTEFACT_NAME}_context_statics")
concrete_sources_direct ("${CONTEXT_STATICS_FILE}")
concrete_require (SCOPE PRIVATE CONCRETE_INTERFACE context)
shared_library_include (SCOPE PUBLIC CONCRETE "${ARTEFACT_NAME}_context_statics")
