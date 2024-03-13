if (NOT WIN32)
    return ()
endif ()

register_concrete (file_system_win32)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT error log memory)
concrete_implements_abstract (file_system)
