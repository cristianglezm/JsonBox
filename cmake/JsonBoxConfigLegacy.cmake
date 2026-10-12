# Before, the package was installed as <libdir>/cmake/JsonBoxConfig.cmake, a place CMake does not
# search, so it was found with -DJsonBox_DIR=<prefix>/lib/cmake. That still works: this file
# hands over to the package in <libdir>/cmake/JsonBox/, which find_package finds by itself.
include("${CMAKE_CURRENT_LIST_DIR}/JsonBox/JsonBoxConfig.cmake")
