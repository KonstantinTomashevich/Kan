register_concrete (virtual_file_system_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE
        ABSTRACT file_system_watcher error hash log memory reflection threading
        CONCRETE_INTERFACE container serialization)

concrete_implements_abstract (virtual_file_system)
# We do not register reflection as it is used only privately.
setup_reflected_preprocessing (SKIP_REFLECTION_REGISTRATION)

set (KAN_VIRTUAL_FILE_SYSTEM_ROPACKH_INITIAL_ITEMS "64" CACHE STRING
        "Initial count of item for read only pack header.")
set (KAN_VIRTUAL_FILE_SYSTEM_ROPACK_DIRECTORY_INITIAL_ITEMS "64" CACHE STRING
        "Initial count of files for read only pack directory hash storage.")
set (KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH "256" CACHE STRING
        "Max length of file name (with extension) for read only pack files.")
set (KAN_VIRTUAL_FILE_SYSTEM_ROPACK_BUILDER_CHUNK_SIZE "1024" CACHE STRING
        "Length of on-stack read buffer for read only pack building.")

concrete_compile_definitions (
        PRIVATE
        KAN_VIRTUAL_FILE_SYSTEM_ROPACKH_INITIAL_ITEMS=${KAN_VIRTUAL_FILE_SYSTEM_ROPACKH_INITIAL_ITEMS}
        KAN_VIRTUAL_FILE_SYSTEM_ROPACK_DIRECTORY_INITIAL_ITEMS=${KAN_VIRTUAL_FILE_SYSTEM_ROPACK_DIRECTORY_INITIAL_ITEMS}
        KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH=${KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH}
        KAN_VIRTUAL_FILE_SYSTEM_ROPACK_BUILDER_CHUNK_SIZE=${KAN_VIRTUAL_FILE_SYSTEM_ROPACK_BUILDER_CHUNK_SIZE})
