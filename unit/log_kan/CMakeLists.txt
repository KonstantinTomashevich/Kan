register_concrete (log_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT error memory threading CONCRETE_INTERFACE container)
concrete_implements_abstract (log)

set (KAN_LOG_CATEGORIES_INITIAL_BUCKETS "16" CACHE STRING
        "Initial count of buckets for interned strings hash storage.")
set (KAN_LOG_CATEGORIES_CALLBACK_ARRAY_INITIAL_SIZE "4" CACHE STRING "Initial size of log callback dynamic array.")

concrete_compile_definitions (
        PRIVATE
        KAN_LOG_CATEGORIES_INITIAL_BUCKETS=${KAN_LOG_CATEGORIES_INITIAL_BUCKETS}
        KAN_LOG_CATEGORIES_CALLBACK_ARRAY_INITIAL_SIZE=${KAN_LOG_CATEGORIES_CALLBACK_ARRAY_INITIAL_SIZE})
c_interface_scanner_setup (GLOB "*.c" "*.h")
