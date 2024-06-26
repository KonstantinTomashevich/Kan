register_concrete (repository_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PRIVATE
        ABSTRACT cpu_dispatch cpu_profiler error log
        CONCRETE_INTERFACE repository_reflection
        THIRD_PARTY qsort)

concrete_implements_abstract (repository)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for storage of singleton storages.")
set (KAN_REPOSITORY_INDEXED_STORAGE_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for storage of indexed storages.")
set (KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for storage of event storages.")
set (KAN_REPOSITORY_MIGRATION_STACK_INITIAL_SIZE "1048576" CACHE STRING
        "Initial size for stack group allocator used for repository migration.")
set (KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE "65536" CACHE STRING
        "Initial size for stack group allocator used for repository switch to serving mode algorithm.")
set (KAN_REPOSITORY_INDEXED_STORAGE_STACK_INITIAL_SIZE "8192" CACHE STRING
        "Initial size for stack group allocator used for temporary allocations for indexed storage algorithms.")
set (KAN_REPOSITORY_VALUE_INDEX_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for value index values hash storage.")
set (KAN_REPOSITORY_UNIQUENESS_WATCHER_INITIAL_BUCKETS "16" CACHE STRING
        "Initial count of buckets for uniqueness watcher for indices without duplication-prevention structure.")
set (KAN_REPOSITORY_UNIQUENESS_WATCHER_LOAD_FACTOR "4" CACHE STRING
        "Target count of records in uniqueness watcher bucket.")
set (KAN_REPOSITORY_UNIQUENESS_WATCHER_SIMPLICITY_LIMIT "8" CACHE STRING
        "Maximum amount of unique records after which uniqueness watcher will switch to hash storage.")

concrete_compile_definitions (
        PRIVATE
        KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS=${KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS}
        KAN_REPOSITORY_INDEXED_STORAGE_INITIAL_BUCKETS=${KAN_REPOSITORY_INDEXED_STORAGE_INITIAL_BUCKETS}
        KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS=${KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS}
        KAN_REPOSITORY_MIGRATION_STACK_INITIAL_SIZE=${KAN_REPOSITORY_MIGRATION_STACK_INITIAL_SIZE}
        KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE=${KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE}
        KAN_REPOSITORY_INDEXED_STORAGE_STACK_INITIAL_SIZE=${KAN_REPOSITORY_INDEXED_STORAGE_STACK_INITIAL_SIZE}
        KAN_REPOSITORY_VALUE_INDEX_INITIAL_BUCKETS=${KAN_REPOSITORY_VALUE_INDEX_INITIAL_BUCKETS}
        KAN_REPOSITORY_VALUE_INDEX_UNIQUE_HASH_INITIAL_BUCKETS=${KAN_REPOSITORY_VALUE_INDEX_UNIQUE_HASH_INITIAL_BUCKETS}
        KAN_REPOSITORY_VALUE_INDEX_UNIQUE_HASH_USE_FACTOR=${KAN_REPOSITORY_VALUE_INDEX_UNIQUE_HASH_USE_FACTOR}
        KAN_REPOSITORY_UNIQUENESS_WATCHER_INITIAL_BUCKETS=${KAN_REPOSITORY_UNIQUENESS_WATCHER_INITIAL_BUCKETS}
        KAN_REPOSITORY_UNIQUENESS_WATCHER_SIMPLICITY_LIMIT=${KAN_REPOSITORY_UNIQUENESS_WATCHER_SIMPLICITY_LIMIT})

option (KAN_REPOSITORY_SAFEGUARDS_ENABLED "Whether safeguard logic for repository multi threaded access is enabled." ON)
if (KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    concrete_compile_definitions (PRIVATE KAN_REPOSITORY_SAFEGUARDS_ENABLED)
endif ()

option (KAN_REPOSITORY_VALIDATION_ENABLED "Whether validation logic for repository usage is enabled." ON)
if (KAN_REPOSITORY_VALIDATION_ENABLED)
    concrete_compile_definitions (PRIVATE KAN_REPOSITORY_VALIDATION_ENABLED)
endif ()
