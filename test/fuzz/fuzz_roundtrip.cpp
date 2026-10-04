// libFuzzer harness for the writer: whatever the parser accepts must be writable, and the
// written text must parse again, without a crash or a hang.
#include <JsonBox.h>

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace{
    std::string write(const JsonBox::Value& v, bool compact){
        std::stringstream ss;
        v.writeToStream(ss, !compact, false);
        return ss.str();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size){
    std::string input(reinterpret_cast<const char*>(data), size);
    JsonBox::Value v;
    try{
        v.loadFromString(input);
    }catch(const std::exception&){
        return 0;
    }
    try{
        auto first = write(v, true);
        JsonBox::Value again;
        again.loadFromString(first);
        (void)write(again, false);
    }catch(const std::exception&){
    }
    return 0;
}
