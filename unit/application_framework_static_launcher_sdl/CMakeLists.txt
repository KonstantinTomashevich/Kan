register_concrete (application_framework_static_launcher_sdl)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PRIVATE
        CONCRETE_INTERFACE application_framework
        THIRD_PARTY debug_break SDL3::SDL3)
concrete_implements_abstract (application_framework_static_launcher)
