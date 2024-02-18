register_concrete (serialization)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT reflection INTERFACE stream)
concrete_require (SCOPE PRIVATE ABSTRACT error log memory threading CONCRETE_INTERFACE container)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_SERIALIZATION_BINARY_SCRIPT_INITIAL_BUCKETS "16" CACHE STRING
        "Initial buckets count for binary serialization script storage.")
set (KAN_SERIALIZATION_BINARY_SCRIPT_LOAD_FACTOR "4" CACHE STRING
        "Target count of binary serialization scripts per bucket.")
set (KAN_SERIALIZATION_BINARY_GENERATION_INITIAL_STACK "65536" CACHE STRING
        "Initial stack size for binary serialization script generation algorithm.")
set (KAN_SERIALIZATION_BINARY_INTERNED_STRING_BUCKETS "16" CACHE STRING
        "Initial buckets count for binary serialization interned string lookup storage.")
set (KAN_SERIALIZATION_BINARY_INTERNED_STRING_LOAD_FACTOR "4" CACHE STRING
        "Target count of binary serialization interned script lookup data per bucket.")
set (KAN_SERIALIZATION_BINARY_INTERNED_STRING_CAPACITY "16" CACHE STRING
        "Initial capacity for interned string lookup generation temporary dynamic array.")
set (KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_BUCKETS "128" CACHE STRING
        "Initial buckets count for binary serialization interned string registry value to index map.")
set (KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_LOAD_FACTOR "4" CACHE STRING
        "Target count of binary serialization interned strings per bucket in value to index map.")

concrete_compile_definitions (
        PRIVATE
        KAN_SERIALIZATION_BINARY_SCRIPT_INITIAL_BUCKETS=${KAN_SERIALIZATION_BINARY_SCRIPT_INITIAL_BUCKETS}
        KAN_SERIALIZATION_BINARY_SCRIPT_LOAD_FACTOR=${KAN_SERIALIZATION_BINARY_SCRIPT_LOAD_FACTOR}
        KAN_SERIALIZATION_BINARY_GENERATION_INITIAL_STACK=${KAN_SERIALIZATION_BINARY_GENERATION_INITIAL_STACK}
        KAN_SERIALIZATION_BINARY_INTERNED_STRING_BUCKETS=${KAN_SERIALIZATION_BINARY_INTERNED_STRING_BUCKETS}
        KAN_SERIALIZATION_BINARY_INTERNED_STRING_LOAD_FACTOR=${KAN_SERIALIZATION_BINARY_INTERNED_STRING_LOAD_FACTOR}
        KAN_SERIALIZATION_BINARY_INTERNED_STRING_CAPACITY=${KAN_SERIALIZATION_BINARY_INTERNED_STRING_CAPACITY}
        KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_BUCKETS=${KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_BUCKETS}
        KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_LOAD_FACTOR=${KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_LOAD_FACTOR}
)
