#include <JsonBox.h>
#include <gtest/gtest.h>

#include <ios>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace{
    using JsonBox::Value;

    // a locale that would write 1234567.5 as 1.234.567,5
    struct CommaPunct : std::numpunct<char>{
        char do_decimal_point() const override{ return ','; }
        char do_thousands_sep() const override{ return '.'; }
        std::string do_grouping() const override{ return "\3"; }
    };

    struct GlobalLocale{
        std::locale previous;
        GlobalLocale() : previous(std::locale::global(std::locale(std::locale::classic(), new CommaPunct))){}
        ~GlobalLocale(){ std::locale::global(previous); }
    };

    // accepts `budget` characters, then every write throws (a full disk, a closed socket...)
    class BudgetBuffer : public std::streambuf{
    public:
        explicit BudgetBuffer(std::size_t budget) : budget(budget){}
    protected:
        int_type overflow(int_type c) override{
            if(used >= budget){
                throw std::runtime_error("no space left");
            }
            ++used;
            return c;
        }
    private:
        std::size_t budget;
        std::size_t used = 0;
    };

    Value document(){
        Value v;
        v["name"] = "a/b";
        v["list"][std::size_t(0)] = 1;
        v["list"][std::size_t(1)] = 2.5;
        v["list"][std::size_t(2)] = "x";
        v["nested"]["deeper"][std::size_t(0)] = true;
        return v;
    }
}

TEST(ValueWriteException, TheStreamGetsItsBufferBackWhateverThePointItFailsAt){
    const Value v = document();
    for(bool indent : {false, true}){
        for(bool escapeAll : {false, true}){
            std::stringstream whole;
            v.writeToStream(whole, indent, escapeAll);
            const std::size_t length = whole.str().size();
            // the filters (indenter, solidus escaper, indent canceller) are installed at different
            // points of the document: fail at every character of it
            for(std::size_t budget = 0; budget < length; ++budget){
                BudgetBuffer destination(budget);
                std::ostream out(&destination);
                out.exceptions(std::ios::badbit);
                EXPECT_THROW(v.writeToStream(out, indent, escapeAll), std::runtime_error)
                    << "indent " << indent << ", escapeAll " << escapeAll << ", budget " << budget;
                EXPECT_TRUE(out.rdbuf() == &destination)
                    << "the stream still points at a filter that is gone: indent " << indent
                    << ", escapeAll " << escapeAll << ", budget " << budget;
            }
        }
    }
}

TEST(ValueWriteException, TheOutputOperatorAloneRestoresTheBufferToo){
    const Value v = document();
    for(std::size_t budget = 0; budget < 40; ++budget){
        BudgetBuffer destination(budget);
        std::ostream out(&destination);
        out.exceptions(std::ios::badbit);
        EXPECT_THROW(out << v, std::runtime_error);
        EXPECT_TRUE(out.rdbuf() == &destination) << "budget " << budget;
    }
}

TEST(ValueWriteException, ANormalWriteStillLeavesTheStreamsOwnBuffer){
    std::stringstream ss;
    auto* before = ss.rdbuf();
    document().writeToStream(ss, true, true);
    document().writeToStream(ss, false, false);
    EXPECT_TRUE(ss.rdbuf() == before);
}

TEST(ValueWriteLocale, IntegersDoNotDependOnTheLocaleOrFlagsOfTheStream){
    std::stringstream grouped;
    grouped.imbue(std::locale(std::locale::classic(), new CommaPunct));
    Value(1234567).writeToStream(grouped, false, false);
    EXPECT_EQ(grouped.str(), "1234567");

    std::stringstream hex;
    hex << std::hex;
    Value(255).writeToStream(hex, false, false);
    EXPECT_EQ(hex.str(), "255");

    std::stringstream array;
    array << std::showpos << std::hex;
    Value v;
    v[std::size_t(0)] = 1000;
    v[std::size_t(1)] = -1000;
    v.writeToStream(array, false, false);
    EXPECT_EQ(array.str(), "[1000,-1000]");
}

TEST(ValueWriteLocale, IntegersDoNotDependOnTheGlobalLocale){
    GlobalLocale locale;
    std::stringstream ss; // takes the global locale
    Value(1234567).writeToStream(ss, false, false);
    EXPECT_EQ(ss.str(), "1234567");
}

TEST(ValueWriteLocale, GetToStringDoesNotDependOnTheGlobalLocale){
    GlobalLocale locale;
    EXPECT_EQ(Value(1234567).getToString(), "1234567");
    EXPECT_EQ(Value(1234.5).getToString(), "1234.5");
}

TEST(ValueParseLocale, NumbersDoNotDependOnTheGlobalLocale){
    GlobalLocale locale;
    Value d;
    d.loadFromString("1234.5");
    ASSERT_TRUE(d.isDouble());
    EXPECT_EQ(d.getDouble(), 1234.5);
    Value e;
    e.loadFromString("[1.5e2,-0.25,7,1234567]");
    ASSERT_EQ(e.getArray().size(), 4u);
    EXPECT_EQ(e.getArray()[0].getDouble(), 150.0);
    EXPECT_EQ(e.getArray()[1].getDouble(), -0.25);
    EXPECT_EQ(e.getArray()[2].getInteger(), 7);
    EXPECT_EQ(e.getArray()[3].getInteger(), 1234567);
}

TEST(ValueWriteLocale, EscapedControlCharactersDoNotDependOnTheGlobalLocale){
    GlobalLocale locale;
    std::stringstream ss;
    Value(std::string("a\x01" "b")).writeToStream(ss, false, false);
    EXPECT_EQ(ss.str(), "\"a\\u0001b\"");
}
