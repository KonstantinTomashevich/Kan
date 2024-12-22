register_abstract (error)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)

option (KAN_ASSERT_ENABLED "Enable assertions from Kan error unit." ON)
if (KAN_ASSERT_ENABLED)
    abstract_compile_definitions (KAN_WITH_ASSERT)
endif ()

abstract_register_implementation (NAME sdl PARTS error_sdl)
