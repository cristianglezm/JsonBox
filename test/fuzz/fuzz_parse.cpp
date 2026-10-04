// libFuzzer harness for JsonBox::Value::loadFromString(). Malformed input must either parse or
// throw a std::exception; it must not crash, hang or touch memory it does not own.
#include <JsonBox.h>

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size){
    std::string input(reinterpret_cast<const char*>(data), size);
    JsonBox::Value v;
    try{
        v.loadFromString(input);
    }catch(const std::exception&){
    }
    return 0;
}
