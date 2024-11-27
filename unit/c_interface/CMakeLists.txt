register_concrete (c_interface)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT error log memory CONCRETE_INTERFACE container INTERFACE stream)

set (KAN_C_INTERFACE_STRING_ENCODING_INITIAL_CAPACITY "32" CACHE STRING
        "Initial capacity for string encoding order dynamic array.")
set (KAN_C_INTERFACE_STRING_ENCODING_INITIAL_BUCKETS "11" CACHE STRING
        "Initial count of buckets for interned string encoding algorithm.")
set (KAN_C_INTERFACE_ENCODED_STRING_MAX_LENGTH "128" CACHE STRING
        "Maximum length of string encoded in serialized c interface.")

concrete_compile_definitions (
        PRIVATE
        KAN_C_INTERFACE_STRING_ENCODING_INITIAL_CAPACITY=${KAN_C_INTERFACE_STRING_ENCODING_INITIAL_CAPACITY}
        KAN_C_INTERFACE_STRING_ENCODING_INITIAL_BUCKETS=${KAN_C_INTERFACE_STRING_ENCODING_INITIAL_BUCKETS}
        KAN_C_INTERFACE_ENCODED_STRING_MAX_LENGTH=${KAN_C_INTERFACE_ENCODED_STRING_MAX_LENGTH})

c_interface_scanner_setup (GLOB "*.c" "*.h")
