register_concrete (memory_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT error threading)
concrete_implements_abstract (memory)

set (KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE "262144" CACHE STRING "Fixed size of a page for paged allocators.")
concrete_compile_definitions (PRIVATE KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE=${KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE})
