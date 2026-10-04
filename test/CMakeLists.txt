file(GLOB JsonBox_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(RunAllTests ${JsonBox_TEST_SOURCES})
target_link_libraries(RunAllTests PRIVATE JsonBox GTest::gtest GTest::gtest_main)
target_compile_features(RunAllTests PRIVATE cxx_std_17)
if(EMSCRIPTEN)
    target_link_options(RunAllTests PRIVATE -fexceptions)
endif()

include(GoogleTest)
if(NOT CMAKE_SYSTEM_NAME MATCHES "Android" AND NOT CMAKE_CROSSCOMPILING_EMULATOR AND NOT EMSCRIPTEN)
    # a test that hangs is a failure, not a stuck CI job
    gtest_discover_tests(RunAllTests PROPERTIES TIMEOUT 30)
elseif(EMSCRIPTEN)
    add_test(NAME RunAllTests COMMAND node $<TARGET_FILE:RunAllTests> )
endif()
