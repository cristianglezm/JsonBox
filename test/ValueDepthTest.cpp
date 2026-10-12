#include <JsonBox.h>
#include <JsonBox/JsonParsingError.h>
#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace{
    const std::size_t limit = JSONBOX_MAX_NESTING_DEPTH;

    std::string arrays(std::size_t depth){
        return std::string(depth, '[') + std::string(depth, ']');
    }
    std::string objects(std::size_t depth){
        std::string text;
        for(std::size_t i = 0; i < depth; ++i){
            text += "{\"a\":";
        }
        text += "1";
        return text + std::string(depth, '}');
    }
    bool throwsParsingError(const std::string& json, std::string* message = nullptr){
        JsonBox::Value v;
        try{
            v.loadFromString(json);
        }catch(const JsonBox::JsonParsingError& e){
            if(message){
                *message = e.what();
            }
            return true;
        }
        return false;
    }
}

TEST(ValueParseDepth, NestingUpToTheLimitIsParsed){
    JsonBox::Value v;
    ASSERT_NO_THROW(v.loadFromString(arrays(limit)));
    EXPECT_TRUE(v.isArray());
    JsonBox::Value o;
    ASSERT_NO_THROW(o.loadFromString(objects(limit)));
    EXPECT_TRUE(o.isObject());
    // the innermost value is still there
    const JsonBox::Value* inner = &o;
    for(std::size_t i = 0; i < limit; ++i){
        inner = &inner->getObject().at("a");
    }
    EXPECT_EQ(inner->getInteger(), 1);
}

TEST(ValueParseDepth, OneLevelMoreThrowsAParsingError){
    std::string message;
    EXPECT_TRUE(throwsParsingError(arrays(limit + 1), &message));
    EXPECT_NE(message.find("nested"), std::string::npos) << message;
    EXPECT_TRUE(throwsParsingError(objects(limit + 1)));
    // mixed
    std::string mixed;
    for(std::size_t i = 0; i < limit + 1; ++i){
        mixed += (i % 2) ? "{\"a\":" : "[";
    }
    EXPECT_TRUE(throwsParsingError(mixed));
}

TEST(ValueParseDepth, VeryDeepInputThrowsInsteadOfOverflowingTheStack){
    EXPECT_TRUE(throwsParsingError(std::string(200000, '[')));
    std::string objectsOnly;
    for(int i = 0; i < 100000; ++i){
        objectsOnly += "{\"a\":";
    }
    EXPECT_TRUE(throwsParsingError(objectsOnly));
}

TEST(ValueParseDepth, SiblingsAreNotNesting){
    std::string wide = "[";
    for(int i = 0; i < 5000; ++i){
        wide += (i ? ",[[]]" : "[[]]");
    }
    wide += "]";
    JsonBox::Value v;
    ASSERT_NO_THROW(v.loadFromString(wide));
    EXPECT_EQ(v.getArray().size(), 5000u);
}

TEST(ValueParseDepth, TheStreamAndConstructorEntryPointsAreLimitedToo){
    std::stringstream tooDeep(arrays(limit + 1));
    JsonBox::Value v;
    EXPECT_THROW(v.loadFromStream(tooDeep), JsonBox::JsonParsingError);
    std::stringstream ok(arrays(limit));
    EXPECT_NO_THROW(JsonBox::Value parsed(ok));
    std::stringstream tooDeepAgain(arrays(limit + 1));
    EXPECT_THROW(JsonBox::Value parsed(tooDeepAgain), JsonBox::JsonParsingError);
}
