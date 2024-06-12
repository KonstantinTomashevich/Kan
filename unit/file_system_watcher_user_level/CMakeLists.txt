register_concrete (file_system_watcher_user_level)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT cpu_dispatch error log memory platform)
concrete_implements_abstract (file_system_watcher)

set (KAN_FILE_SYSTEM_WATCHER_UL_MIN_DELAY_NS "100000000" CACHE STRING
        "Minimum delay between user level file system watcher pools.")

concrete_compile_definitions (
        PRIVATE
        KAN_FILE_SYSTEM_WATCHER_UL_MIN_DELAY_NS=${KAN_FILE_SYSTEM_WATCHER_UL_MIN_DELAY_NS})
