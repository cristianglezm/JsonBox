#include <JsonBox.h>

#include <iostream>
#include <sstream>

int main(){
    JsonBox::Value value;
    value.loadFromString("{\"name\":\"JsonBox\",\"list\":[1,2.5,true]}");
    std::stringstream out;
    value.writeToStream(out, false);
    std::cout << out.str() << '\n';
    return (value["list"].getArray().size() == 3 && value["name"].getString() == "JsonBox") ? 0 : 1;
}
