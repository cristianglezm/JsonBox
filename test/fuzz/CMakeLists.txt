# libFuzzer harnesses for JsonBox's parser. Only built when JsonBox_BUILD_FUZZERS is ON,
# see README.md for how to run them.

set(JsonBox_FUZZ_TARGETS
    fuzz_parse
    fuzz_roundtrip
)

foreach(fuzzTarget ${JsonBox_FUZZ_TARGETS})
    add_executable(${fuzzTarget} ${fuzzTarget}.cpp)
    target_compile_options(${fuzzTarget} PRIVATE -fsanitize=fuzzer,address,undefined -g -O1)
    target_link_options(${fuzzTarget} PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(${fuzzTarget} PRIVATE JsonBox)
    set_target_properties(${fuzzTarget} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz/bin"
    )
endforeach()
