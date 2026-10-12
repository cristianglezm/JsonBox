#include <JsonBox.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace{
    using JsonBox::Value;

    Value parse(const std::string& text){
        Value v;
        v.loadFromString(text);
        return v;
    }
    std::string write(const Value& v){
        std::stringstream ss;
        v.writeToStream(ss, false, false);
        return ss.str();
    }
}

TEST(ValueParseNumber, WholeNumbersThatFitAnIntStayIntegers){
    for(const char* text : {"0", "1", "-1", "2147483647", "-2147483648", "123456789"}){
        Value v = parse(text);
        EXPECT_TRUE(v.isInteger()) << text;
        EXPECT_EQ(v.getDouble(), std::stod(text)) << text;
    }
    EXPECT_EQ(parse("2147483647").getInteger(), std::numeric_limits<int>::max());
    EXPECT_EQ(parse("-2147483648").getInteger(), std::numeric_limits<int>::min());
}

TEST(ValueParseNumber, WholeNumbersThatDoNotFitAnIntAreReadAsDoubles){
    struct Case{ const char* text; double value; };
    const Case cases[] = {
        {"2147483648", 2147483648.0}, {"-2147483649", -2147483649.0}, {"4294967296", 4294967296.0},
        {"-3000000000", -3000000000.0}, {"123456789012", 123456789012.0},
        {"999999999999999", 999999999999999.0}, {"1234567890123456", 1234567890123456.0},
        {"9007199254740992", 9007199254740992.0}
    };
    for(const auto& c : cases){
        Value v = parse(c.text);
        EXPECT_TRUE(v.isDouble()) << c.text;
        EXPECT_EQ(v.getDouble(), c.value) << c.text;
    }
}

TEST(ValueParseNumber, ATokenTooLongForADoubleStillIsNotAnInteger){
    Value v = parse("1" + std::string(400, '0'));
    EXPECT_TRUE(v.isDouble());
    EXPECT_GT(v.getDouble(), 1e300);
    EXPECT_FALSE(std::isinf(v.getDouble()));
    Value negative = parse("-" + std::string(30, '9'));
    EXPECT_TRUE(negative.isDouble());
    EXPECT_LT(negative.getDouble(), -9e29);
}

TEST(ValueParseNumber, GetIntegerOfSuchANumberStillClamps){
    EXPECT_EQ(parse("3000000000").getInteger(), std::numeric_limits<int>::max());
    EXPECT_EQ(parse("-3000000000").getInteger(), std::numeric_limits<int>::min());
}

TEST(ValueParseNumber, AWholeDoubleOfAnySizeSurvivesAWriteAndARead){
    for(double d : {2147483648.0, 3.0e9, -3.0e9, 4294967296.0, 123456789012.0, 999999999999999.0, 1.0e15,
                    1234567890123456.0, 9007199254740992.0, 1.5e17, 1.0e300, 2147483647.0, -2147483648.0}){
        Value back = parse(write(Value(d)));
        EXPECT_EQ(back.getDouble(), d) << write(Value(d));
    }
}

TEST(ValueParseNumber, AParsedNumberIsWrittenAsItWasRead){
    for(const char* text : {"3000000000", "-3000000000", "123456789012", "2147483647", "-2147483648", "7"}){
        EXPECT_EQ(write(parse(text)), text);
    }
}

TEST(ValueParseNumber, TheSameInsideArraysAndObjects){
    Value v = parse("{\"big\":3000000000,\"list\":[1,4294967296,-3000000000,2],\"small\":5}");
    EXPECT_EQ(v["big"].getDouble(), 3.0e9);
    ASSERT_EQ(v["list"].getArray().size(), 4u);
    EXPECT_EQ(v["list"].getArray()[0].getInteger(), 1);
    EXPECT_EQ(v["list"].getArray()[1].getDouble(), 4294967296.0);
    EXPECT_EQ(v["list"].getArray()[2].getDouble(), -3.0e9);
    EXPECT_EQ(v["list"].getArray()[3].getInteger(), 2);
    EXPECT_TRUE(v["small"].isInteger());
}

TEST(ValueParseNumber, WhatWasNotAnOverflowIsAsBefore){
    // no digits: a failed conversion, read as 0 (the parser is lenient here, and stays so)
    Value minus = parse("-");
    EXPECT_TRUE(minus.isInteger());
    EXPECT_EQ(minus.getInteger(), 0);
    EXPECT_TRUE(parse("1.5").isDouble());
    EXPECT_TRUE(parse("1e3").isDouble());
    EXPECT_EQ(parse("1e3").getDouble(), 1000.0);
    EXPECT_TRUE(parse("-0").isInteger());
}
