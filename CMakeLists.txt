cmake_minimum_required (VERSION 3.21)
cmake_policy (VERSION 3.21)

project (
        Kan
        DESCRIPTION "Game development framework for pure C programming."
        # C++ is needed in cases where there are no C bindings for required libraries.
        LANGUAGES C CXX)

# 17th standard seems to be the best bet for both usability and compiler support.
set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 17)

# Include CMake Unit Framework.
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeUnitFramework/CodeGeneration.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeUnitFramework/Core.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeUnitFramework/Product.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeUnitFramework/Unit.cmake")

# Configure CMake Unit Framework.
set (UNIT_FRAMEWORK_API_CASE "mixed_snake_case")

# Include Kan CMake extensions.
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/c_interface_scanner.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/clang_format.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/context.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/global_options.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/reflection_generator.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/testing.cmake")

# Must be called from root CMakeLists.txt, otherwise CTest configuration won't be placed into binary root.
enable_testing ()

add_subdirectory (third_party)
add_subdirectory (unit)
add_subdirectory (test)
add_subdirectory (executable)
