cmake_minimum_required (VERSION 3.25)
cmake_policy (VERSION 3.25)

project (
        Kan
        DESCRIPTION "Game development framework for pure C programming."
        # C++ is needed in cases where there are no C bindings for required libraries.
        LANGUAGES C CXX)

# Include CMake Unit Framework.
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeUnitFramework/CodeGeneration.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeUnitFramework/Core.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeUnitFramework/Product.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeUnitFramework/Unit.cmake")

# Configure CMake Unit Framework.
set (UNIT_FRAMEWORK_API_CASE "mixed_snake_case")

# Include Kan CMake extensions.
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/application.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/binary_directory_flattening.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/clang_format.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/context.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/global_options.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/reflection_helpers.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/reflection_preprocessor.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/reflection_statics.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/testing.cmake")
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake/kan/universe.cmake")

# Must be called from root CMakeLists.txt, otherwise CTest configuration won't be placed into binary root.
enable_testing ()

add_subdirectory (third_party)

# We use C23 standard as it is required for Cushion defer feature.
# However, MSVC/ClangCL do not mark themselves as C23 compliant, 
# so we ignore that setting there and use clatest with them instead.
if (NOT MSVC)
    set (CMAKE_C_STANDARD 23)
    set (CMAKE_C_STANDARD_REQUIRED ON)
else ()
    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^.*Clang$")
        add_compile_options ("$<IF:$<COMPILE_LANGUAGE:C>,-Xclang=-std=c23,>")
    else ()
        add_compile_options ("$<IF:$<COMPILE_LANGUAGE:C>,/std:clatest,>")
    endif ()
endif ()
set (CMAKE_CXX_STANDARD 17)

add_subdirectory (unit)
add_subdirectory (test)
add_subdirectory (executable)
