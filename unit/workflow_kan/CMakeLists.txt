register_concrete (workflow_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT error log memory reflection threading CONCRETE_INTERFACE container)
concrete_implements_abstract (workflow)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_WORKFLOW_GRAPH_NODES_INITIAL_BUCKETS "67" CACHE STRING
        "Initial count of buckets for storing workflow graph nodes during build process.")
set (KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY "8" CACHE STRING
        "Initial capacity of arrays inside node that store accesses and dependencies.")
set (KAN_WORKFLOW_RESOURCE_INITIAL_BUCKETS "67" CACHE STRING
        "Initial count of buckets for storing registered resources during verification.")
set (KAN_WORKFLOW_VERIFICATION_STACK_SIZE "524288" CACHE STRING
        "Size of one stack for verification algorithm stack group allocator.")
set (KAN_WORKFLOW_EXECUTION_STACK_SIZE "65536" CACHE STRING
        "Size of one stack for workflow execution algorithm stack group allocator.")

concrete_compile_definitions (
        PRIVATE
        KAN_WORKFLOW_GRAPH_NODES_INITIAL_BUCKETS=${KAN_WORKFLOW_GRAPH_NODES_INITIAL_BUCKETS}
        KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY=${KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY}
        KAN_WORKFLOW_RESOURCE_INITIAL_BUCKETS=${KAN_WORKFLOW_RESOURCE_INITIAL_BUCKETS}
        KAN_WORKFLOW_VERIFICATION_STACK_SIZE=${KAN_WORKFLOW_VERIFICATION_STACK_SIZE}
        KAN_WORKFLOW_EXECUTION_STACK_SIZE=${KAN_WORKFLOW_EXECUTION_STACK_SIZE})

option (KAN_WORKFLOW_VERIFY "Whether workflow verification logic is turned on." ON)
if (KAN_WORKFLOW_VERIFY)
    concrete_compile_definitions (PRIVATE KAN_WORKFLOW_VERIFY)
endif ()
