#include <JsonBox.h>
#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace{
    JsonBox::Value parse(const std::string& json){
        JsonBox::Value v;
        v.loadFromString(json);
        return v;
    }
    std::string write(const JsonBox::Value& v, bool indent = false){
        std::stringstream ss;
        v.writeToStream(ss, indent, false);
        return ss.str();
    }
    // Delivers `data`, then throws instead of reporting the end of the input. istream::get
    // catches that and sets badbit, never eofbit: a read error, as opposed to a short input.
    struct FailingBuf : std::streambuf{
        explicit FailingBuf(std::string d) : data(std::move(d)){
            setg(data.data(), data.data(), data.data() + data.size());
        }
        int_type underflow() override{
            throw std::runtime_error("read error");
        }
        std::string data;
    };
}

TEST(ValueParse, Scalars){
    EXPECT_TRUE(parse("true").getBoolean());
    EXPECT_FALSE(parse("false").getBoolean());
    EXPECT_TRUE(parse("null").isNull());
    EXPECT_EQ(parse("\"abc\"").getString(), "abc");
    EXPECT_EQ(parse("42").getInteger(), 42);
    EXPECT_EQ(parse("-17").getInteger(), -17);
    EXPECT_DOUBLE_EQ(parse("2.5").getDouble(), 2.5);
    EXPECT_DOUBLE_EQ(parse("-1.25e2").getDouble(), -125.0);
}

TEST(ValueParse, ANumberAtTheEndOfTheInputKeepsItsLastDigit){
    // the last character used to be read twice once the input ran out
    EXPECT_EQ(parse("12").getInteger(), 12);
    EXPECT_EQ(parse("-5").getInteger(), -5);
    EXPECT_DOUBLE_EQ(parse("1.5").getDouble(), 1.5);
    EXPECT_DOUBLE_EQ(parse("3e2").getDouble(), 300.0);
}

TEST(ValueParse, AOneCharacterInputIsNotMistakenForUTF16){
    EXPECT_EQ(parse("7").getInteger(), 7);
}

TEST(ValueParse, EmptyAndWhitespaceInputDoNotThrow){
    EXPECT_NO_THROW(parse(""));
    EXPECT_NO_THROW(parse("   "));
}

TEST(ValueParse, ObjectsAndArraysNest){
    auto v = parse(R"({"a":[1,2.5,"x",true,null,{"b":[]}],"c":"d"})");
    ASSERT_TRUE(v.isObject());
    const auto& a = v["a"].getArray();
    ASSERT_EQ(a.size(), 6u);
    EXPECT_EQ(a[0].getInteger(), 1);
    EXPECT_DOUBLE_EQ(a[1].getDouble(), 2.5);
    EXPECT_EQ(a[2].getString(), "x");
    EXPECT_TRUE(a[3].getBoolean());
    EXPECT_TRUE(a[4].isNull());
    EXPECT_TRUE(v["a"][5]["b"].isArray());
    EXPECT_EQ(v["c"].getString(), "d");
}

TEST(ValueParse, WhitespaceBetweenTokensIsIgnored){
    auto v = parse(" {\n \"a\" :\t[ 1 , 2 ] \r\n} ");
    ASSERT_TRUE(v["a"].isArray());
    EXPECT_EQ(v["a"].getArray().size(), 2u);
}

TEST(ValueParse, StringEscapes){
    EXPECT_EQ(parse(R"("a\"b\\c\/d\n\t")").getString(), "a\"b\\c/d\n\t");
    EXPECT_EQ(parse(R"("\u00e9")").getString(), "\xC3\xA9");
    // a surrogate pair is one code point
    EXPECT_EQ(parse(R"("\ud83d\ude00")").getString(), "\xF0\x9F\x98\x80");
}

TEST(ValueParse, UTF16InputIsRejected){
    EXPECT_THROW(parse(std::string("\0a\0b", 4)), std::exception);
}

TEST(ValueWrite, RoundTripsThroughTheWriter){
    const std::string json = R"({"a":[1,2.5,"x",true,null,{"b":[]}],"c":"d"})";
    auto first = write(parse(json));
    EXPECT_EQ(write(parse(first)), first);
    auto indented = write(parse(json), true);
    EXPECT_EQ(write(parse(indented)), first);
}

TEST(ValueWrite, EscapesWhatTheParserUnescapes){
    JsonBox::Value v;
    v = std::string("a\"b\\c\n");
    EXPECT_EQ(parse(write(v)).getString(), "a\"b\\c\n");
}

// Truncated input over a stream that fails without reaching its end used to loop forever
// in every parsing loop that only tested for eof.
TEST(ValueParseFailingStream, DoesNotHang){
    for(const char* input : {"[1,", "[1", "[[", "{\"a\":", "{\"a\":[1,", "\"abc", "\"abc\\", "\"\\u12", "12", "-", "tr", "nu"}){
        FailingBuf buf(input);
        std::istream in(&buf);
        JsonBox::Value v;
        try{
            v.loadFromStream(in);
        }catch(const std::exception&){
        }
        SUCCEED() << input;
    }
}

TEST(ValueParseFailingStream, ValuesReadBeforeTheErrorAreKept){
    FailingBuf buf("[1,2,");
    std::istream in(&buf);
    JsonBox::Value v;
    v.loadFromStream(in);
    ASSERT_TRUE(v.isArray());
    EXPECT_EQ(v.getArray().size(), 2u);
}

TEST(ValueParseTruncated, EveryPrefixOfADocumentTerminates){
    const std::string json = R"({"a":[1,-2.5e3,"x\u00e9\n",true,null,{"b":[[],{}]}],"c":"d"})";
    for(std::size_t n = 0; n <= json.size(); ++n){
        JsonBox::Value v;
        try{
            v.loadFromString(json.substr(0, n));
        }catch(const std::exception&){
        }
    }
    SUCCEED();
}
