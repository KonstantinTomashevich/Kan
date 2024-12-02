if (NOT WIN32)
    return ()
endif ()

register_concrete (file_system_win32)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT error log memory precise_time)
concrete_implements_abstract (file_system)

set (KAN_FILE_SYSTEM_WIN32_LOCK_FILE_WAIT_NS "500000000" CACHE STRING
        "Time in nanoseconds to wait between attempts to acquire lock file in directory.")
concrete_compile_definitions (
        PRIVATE
        KAN_FILE_SYSTEM_WIN32_LOCK_FILE_WAIT_NS=${KAN_FILE_SYSTEM_WIN32_LOCK_FILE_WAIT_NS})
