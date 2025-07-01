register_concrete (log_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT error memory threading CONCRETE_INTERFACE container)
setup_core_preprocessing ()
concrete_implements_abstract (log)

set (KAN_LOG_CATEGORIES_INITIAL_BUCKETS "17" CACHE STRING
        "Initial count of buckets for interned strings hash storage.")
set (KAN_LOG_CATEGORIES_CALLBACK_ARRAY_INITIAL_SIZE "4" CACHE STRING "Initial size of log callback dynamic array.")
set (KAN_LOG_MAX_FORMATTING_BUFFER_SIZE "65536" CACHE STRING "Maximum supported size for formatting buffer.")

concrete_compile_definitions (
        PRIVATE
        KAN_LOG_CATEGORIES_INITIAL_BUCKETS=${KAN_LOG_CATEGORIES_INITIAL_BUCKETS}
        KAN_LOG_CATEGORIES_CALLBACK_ARRAY_INITIAL_SIZE=${KAN_LOG_CATEGORIES_CALLBACK_ARRAY_INITIAL_SIZE}
        KAN_LOG_MAX_FORMATTING_BUFFER_SIZE=${KAN_LOG_MAX_FORMATTING_BUFFER_SIZE})
