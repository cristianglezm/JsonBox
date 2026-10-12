# A project that uses JsonBox in each of the ways it supports. Built by CI (and by hand) against an
# install of JsonBox, see .github/workflows/ci.yml:
#   cmake -S test/package -B build-package -DJSONBOX_CONSUME=<mode> -DCMAKE_PREFIX_PATH=<install>
cmake_minimum_required(VERSION 3.13)
project(JsonBoxPackageTest CXX)

set(JSONBOX_CONSUME "config" CACHE STRING
    "config | old-target | old-dir | module | subdirectory | wrong-version")
set(JSONBOX_SOURCE_DIR "" CACHE PATH "the JsonBox sources (for module and subdirectory)")
set(JSONBOX_PREFIX "" CACHE PATH "where JsonBox is installed (for old-dir and module)")

add_executable(consumer main.cpp)

if(JSONBOX_CONSUME STREQUAL "config")
    # what a project does: the namespaced target, with a version
    find_package(JsonBox 0.6 CONFIG REQUIRED)
    target_link_libraries(consumer PRIVATE JsonBox::JsonBox)

elseif(JSONBOX_CONSUME STREQUAL "old-target")
    # the target used to be called JsonBox
    find_package(JsonBox CONFIG REQUIRED)
    target_link_libraries(consumer PRIVATE JsonBox)

elseif(JSONBOX_CONSUME STREQUAL "old-dir")
    # the package used to be found by pointing JsonBox_DIR at <prefix>/lib/cmake
    set(JsonBox_DIR "${JSONBOX_PREFIX}/lib/cmake" CACHE PATH "" FORCE)
    find_package(JsonBox CONFIG REQUIRED)
    target_link_libraries(consumer PRIVATE JsonBox)

elseif(JSONBOX_CONSUME STREQUAL "module")
    # FindJsonBox.cmake, as EvoAI and the docs used it
    list(APPEND CMAKE_MODULE_PATH "${JSONBOX_SOURCE_DIR}/cmake/Modules")
    set(JsonBox_ROOT "${JSONBOX_PREFIX}")
    find_package(JsonBox REQUIRED)
    target_include_directories(consumer PRIVATE ${JsonBox_INCLUDE_DIR})
    target_link_libraries(consumer PRIVATE ${JsonBox_LIBRARIES})

elseif(JSONBOX_CONSUME STREQUAL "subdirectory")
    set(JsonBox_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(JsonBox_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${JSONBOX_SOURCE_DIR}" jsonbox)
    target_link_libraries(consumer PRIVATE JsonBox::JsonBox)

elseif(JSONBOX_CONSUME STREQUAL "wrong-version")
    # a version that is not installed must not be found
    find_package(JsonBox 99.0 CONFIG QUIET)
    if(JsonBox_FOUND)
        message(FATAL_ERROR "find_package(JsonBox 99.0) found JsonBox ${JsonBox_VERSION}")
    endif()
    message(STATUS "find_package(JsonBox 99.0) did not find it, as it should")
    return()

else()
    message(FATAL_ERROR "unknown JSONBOX_CONSUME '${JSONBOX_CONSUME}'")
endif()
