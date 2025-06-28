register_concrete (reflection_preprocessor_main)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT error file_system memory CONCRETE_INTERFACE container)

set (KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY "131072" CACHE STRING
        "Initial capacity of section generation buffer for generated reflection sections.")
set (KAN_REFLECTION_PREPROCESSOR_IO_BUFFER "131072" CACHE STRING
        "Size of a buffer for IO operations for reflection preprocessor.")
set (KAN_REFLECTION_PREPROCESSOR_STACK_ALLOCATOR_ITEM "131072" CACHE STRING
        "Size of one stack of stack group allocator used for various persistent allocations.")
set (KAN_REFLECTION_PREPROCESSOR_TARGET_FILE_BUCKETS "17" CACHE STRING
        "Initial count of buckets for target files.")
set (KAN_REFLECTION_PREPROCESSOR_INCLUDED_FILE_BUCKETS "17" CACHE STRING
        "Initial count of buckets for included files.")

concrete_compile_definitions (
        PRIVATE
        KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY=${KAN_REFLECTION_PREPROCESSOR_SECTION_CAPACITY}
        KAN_REFLECTION_PREPROCESSOR_IO_BUFFER=${KAN_REFLECTION_PREPROCESSOR_IO_BUFFER}
        KAN_REFLECTION_PREPROCESSOR_STACK_ALLOCATOR_ITEM=${KAN_REFLECTION_PREPROCESSOR_STACK_ALLOCATOR_ITEM}
        KAN_REFLECTION_PREPROCESSOR_TARGET_FILE_BUCKETS=${KAN_REFLECTION_PREPROCESSOR_TARGET_FILE_BUCKETS}
        KAN_REFLECTION_PREPROCESSOR_INCLUDED_FILE_BUCKETS=${KAN_REFLECTION_PREPROCESSOR_INCLUDED_FILE_BUCKETS})

concrete_preprocessing_queue_step_apply (COMMAND re2c ARGUMENTS "$$INPUT" -o "$$OUTPUT")
setup_core_preprocessing ()
