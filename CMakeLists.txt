cmake_minimum_required(VERSION 3.27)
project(prefix_sum)

#
# Set language version used
#

# At the moment only C++23 is tested.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
# Libc++ doesn't support compiler extensions for modules.
set(CMAKE_CXX_EXTENSIONS OFF)

#
# Enable modules in CMake
#

# This is required to write your own modules in your project.
if(CMAKE_VERSION VERSION_LESS "3.27.0")
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "2182bf5c-ef0d-489a-91da-49dbc3090d2a")
else()
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "aa1f7df0-828a-4fcd-9afc-2dc80491aca7")
endif()
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)

#
# Import the modules from libc++
#

include(FetchContent)
FetchContent_Declare(
        std
        URL "file://${LIBCXX_BUILD}/modules/c++/v1/"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(std)
if(NOT std_POPULATED)
    FetchContent_Populate(std)
    add_subdirectory(${std_SOURCE_DIR} ${std_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

#
# Adjust project compiler flags
#

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fprebuilt-module-path=${CMAKE_BINARY_DIR}/_deps/std-build/CMakeFiles/std.dir/>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>)
# The include path needs to be set to be able to use macros from headers.
# For example from, the headers <cassert> and <version>.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-isystem>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${LIBCXX_BUILD}/include/c++/v1>)

#
# Adjust project linker flags
#

add_link_options($<$<COMPILE_LANGUAGE:CXX>:-nostdlib++>)
add_link_options($<$<COMPILE_LANGUAGE:CXX>:-L${LIBCXX_BUILD}/lib>)
add_link_options($<$<COMPILE_LANGUAGE:CXX>:-Wl,-rpath,${LIBCXX_BUILD}/lib>)
# Linking against std is required for CMake to get the proper dependencies
link_libraries(std c++)

include(FetchContent)
FetchContent_Declare(
    BS_thread_pool
    GIT_REPOSITORY https://github.com/stripe2933/thread-pool.git
    GIT_TAG master
)
FetchContent_MakeAvailable(BS_thread_pool)

add_executable(prefix_sum main.cpp)

add_library(benchmark)
target_sources(benchmark
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
            benchmark.cppm
)

target_link_libraries(prefix_sum PRIVATE BS_thread_pool benchmark)
