register_concrete (reflection_generator_main)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT error file_system memory CONCRETE_INTERFACE c_interface container)

set (KAN_REFLECTION_GENERATOR_FUNCTORS_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for functor registry hash storage.")

concrete_compile_definitions (
        PRIVATE
        KAN_REFLECTION_GENERATOR_FUNCTORS_INITIAL_BUCKETS=${KAN_REFLECTION_GENERATOR_FUNCTORS_INITIAL_BUCKETS})
