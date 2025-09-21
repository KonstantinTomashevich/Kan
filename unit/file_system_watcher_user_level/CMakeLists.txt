register_concrete (file_system_watcher_user_level)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT error log memory platform precise_time threading)
setup_core_preprocessing ()
concrete_implements_abstract (file_system_watcher)

set (KAN_FILE_SYSTEM_WATCHER_UL_WAKE_UP_DELTA_NS "32000000" CACHE STRING
        "Delay between wake ups of thread that updates file system watchers.")

concrete_compile_definitions (
        PRIVATE
        KAN_FILE_SYSTEM_WATCHER_UL_WAKE_UP_DELTA_NS=${KAN_FILE_SYSTEM_WATCHER_UL_WAKE_UP_DELTA_NS})
