cmake_minimum_required (VERSION 3.21)
cmake_policy (VERSION 3.21)

project (
    Kan
    DESCRIPTION "Game development framework for pure C programming."
    # C++ is needed in cases where there are no C bindings for required libraries.
    LANGUAGES C CXX)

# 17th standard seems to be the best bet for both usability and compiler support.
set (CMAKE_C_STANDARD 17)
set (CMAKE_CXX_STANDARD 17)

# Include CMake Unit Framework.
include ("${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeUnitFramework/CodeGeneration.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeUnitFramework/Core.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeUnitFramework/Product.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeUnitFramework/Unit.cmake")

# Include Kan CMake extensions.
# Nothing here for now.

# Must be called from root CMakeLists.txt, otherwise CTest configuration won't be placed into binary root.
enable_testing ()

add_subdirectory (ThirdParty)
add_subdirectory (Unit)
add_subdirectory (Test)
add_subdirectory (Executable)
