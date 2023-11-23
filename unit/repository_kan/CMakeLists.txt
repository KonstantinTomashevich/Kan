register_concrete (repository_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT cpu_dispatch error log CONCRETE_INTERFACE repository_reflection)
concrete_implements_abstract (repository)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for storage of singleton storages.")
set (KAN_REPOSITORY_SINGLETON_STORAGE_LOAD_FACTOR "4" CACHE STRING "Target count of singleton storages in a bucket.")

set (KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for storage of event storages.")
set (KAN_REPOSITORY_EVENT_STORAGE_LOAD_FACTOR "4" CACHE STRING "Target count of event storages in a bucket.")

set (KAN_REPOSITORY_MIGRATION_STACK_INITIAL_SIZE "1048576" CACHE STRING
        "Initial size for stack group allocator used for repository migration.")

concrete_compile_definitions (
        PRIVATE
        KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS=${KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS}
        KAN_REPOSITORY_SINGLETON_STORAGE_LOAD_FACTOR=${KAN_REPOSITORY_SINGLETON_STORAGE_LOAD_FACTOR}
        KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS=${KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS}
        KAN_REPOSITORY_EVENT_STORAGE_LOAD_FACTOR=${KAN_REPOSITORY_EVENT_STORAGE_LOAD_FACTOR}
        KAN_REPOSITORY_MIGRATION_STACK_INITIAL_SIZE=${KAN_REPOSITORY_MIGRATION_STACK_INITIAL_SIZE})

option (KAN_REPOSITORY_SAFEGUARDS_ENABLED "Whether safeguard logic for repository multi threaded access is enabled." ON)
if (KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    concrete_compile_definitions (PRIVATE KAN_REPOSITORY_SAFEGUARDS_ENABLED)
endif ()
