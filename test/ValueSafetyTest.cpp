#include <JsonBox.h>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

// Allocation failure injection: while armed, the Nth allocation throws std::bad_alloc. Inert
// otherwise. (It replaces operator new for the whole test executable.)
namespace{
    long failAt = -1;
    long allocations = 0;
}

void* operator new(std::size_t size){
    if(failAt >= 0 && allocations++ == failAt){
        throw std::bad_alloc();
    }
    void* p = std::malloc(size ? size : 1);
    if(!p){
        throw std::bad_alloc();
    }
    return p;
}
// every form is replaced, so that what one form allocates the matching delete frees (GoogleTest
// allocates with the nothrow form)
void* operator new[](std::size_t size){ return operator new(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept{ return std::malloc(size ? size : 1); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept{ return std::malloc(size ? size : 1); }
void operator delete(void* p) noexcept{ std::free(p); }
void operator delete[](void* p) noexcept{ std::free(p); }
void operator delete(void* p, std::size_t) noexcept{ std::free(p); }
void operator delete[](void* p, std::size_t) noexcept{ std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept{ std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept{ std::free(p); }

namespace{
    using JsonBox::Value;
    using JsonBox::Array;
    using JsonBox::Object;

    bool injectionWorks(){
        allocations = 0;
        failAt = 0;
        bool threw = false;
        try{
            // through a volatile pointer, or an optimizer may drop the allocation altogether
            int* volatile probe = new int(1);
            delete probe;
        }catch(const std::bad_alloc&){
            threw = true;
        }
        failAt = -1;
        return threw;
    }

    // Runs `operation` on a copy of `original`, failing the 0th, 1st, 2nd... allocation inside it,
    // until it finishes. A failed operation must leave the value as it was (and destroying it must
    // not touch freed memory: that is what the sanitizers are for). Returns the failures injected.
    // (mustBeUnchanged = false only checks that the value is still valid: it is destroyed below)
    int forEveryFailurePoint(const Value& original, const std::function<void(Value&)>& operation,
                             bool mustBeUnchanged = true){
        int failures = 0;
        for(long point = 0; point < 64; ++point){
            Value value = original;
            bool completed = false;
            allocations = 0;
            failAt = point;
            try{
                operation(value);
                completed = true;
            }catch(const std::bad_alloc&){
                ++failures;
            }
            failAt = -1;
            if(!completed){
                EXPECT_TRUE(!mustBeUnchanged || value == original) << "allocation " << point << " failed and the value changed";
            }else{
                break;
            }
        }
        return failures;
    }

    Value longString(){
        return Value(std::string(200, 'x'));
    }
    Value sample(){
        Value v;
        v["list"][std::size_t(0)] = std::string(100, 'y');
        v["list"][std::size_t(1)] = 2.5;
        v["name"] = std::string(100, 'z');
        return v;
    }
}

TEST(ValueExceptionSafety, CopyAssignmentLeavesTheTargetAloneWhenAnAllocationFails){
    if(!injectionWorks()){ GTEST_SKIP() << "operator new cannot be replaced in this build"; }
    Value source = sample();
    EXPECT_GT(forEveryFailurePoint(longString(), [&](Value& v){ v = source; }), 0);
    EXPECT_GT(forEveryFailurePoint(sample(), [&](Value& v){ v = source; }), 0);
    EXPECT_GT(forEveryFailurePoint(Value(1), [&](Value& v){ v = source; }), 0);
}

TEST(ValueExceptionSafety, SettersLeaveTheValueAloneWhenAnAllocationFails){
    if(!injectionWorks()){ GTEST_SKIP() << "operator new cannot be replaced in this build"; }
    Object object;
    object["k"] = Value(std::string(100, 'k'));
    Array array;
    array.push_back(Value(std::string(100, 'a')));
    const std::string text(100, 't');

    EXPECT_GT(forEveryFailurePoint(sample(), [&](Value& v){ v.setString(text); }), 0);
    EXPECT_GT(forEveryFailurePoint(sample(), [&](Value& v){ v.setInteger(1); }), 0);
    EXPECT_GT(forEveryFailurePoint(sample(), [&](Value& v){ v.setDouble(1.5); }), 0);
    EXPECT_GT(forEveryFailurePoint(sample(), [&](Value& v){ v.setBoolean(true); }), 0);
    EXPECT_GT(forEveryFailurePoint(longString(), [&](Value& v){ v.setObject(object); }), 0);
    EXPECT_GT(forEveryFailurePoint(longString(), [&](Value& v){ v.setArray(array); }), 0);
    EXPECT_GT(forEveryFailurePoint(sample(), [&](Value& v){ v = text; }), 0);
    // the value becomes an object before the key is inserted: valid, not necessarily unchanged
    EXPECT_GT(forEveryFailurePoint(longString(), [&](Value& v){ v["key"] = 1; }, false), 0);
    EXPECT_GT(forEveryFailurePoint(longString(), [&](Value& v){ (void) v[3]; }), 0);
}

TEST(ValueIndex, AHugeIndexIsRejected){
    const auto huge = std::numeric_limits<std::size_t>::max();
    Value fresh;
    EXPECT_THROW(fresh[huge] = 1, std::out_of_range);
    EXPECT_TRUE(fresh.isNull()) << "a rejected index must not turn the value into an array";

    Value array;
    array[1] = 5;
    EXPECT_THROW(array[huge], std::out_of_range);
    EXPECT_THROW(array[Array().max_size()], std::out_of_range);
    EXPECT_EQ(array.getArray().size(), 2u);

    Value text("kept");
    EXPECT_THROW(text[huge], std::out_of_range);
    EXPECT_EQ(text.getString(), "kept");
}

TEST(ValueIndex, OrdinaryIndexesStillGrowTheArray){
    Value v;
    v[3] = 7;
    ASSERT_TRUE(v.isArray());
    EXPECT_EQ(v.getArray().size(), 4u);
    EXPECT_EQ(v[3].getInteger(), 7);
    v[5] = 1;
    EXPECT_EQ(v.getArray().size(), 6u);
}

TEST(ValueConstruction, ANullCStringIsNull){
    const char* none = nullptr;
    Value constructed(none);
    EXPECT_TRUE(constructed.isNull());
    Value assigned("text");
    assigned = none;
    EXPECT_TRUE(assigned.isNull());
    EXPECT_EQ(Value("text").getString(), "text");
}

TEST(ValueInteger, ADoubleOutOfRangeIsClampedAndNaNIsTheDefault){
    const int max = std::numeric_limits<int>::max();
    const int min = std::numeric_limits<int>::min();
    EXPECT_EQ(Value(1e300).getInteger(), max);
    EXPECT_EQ(Value(-1e300).getInteger(), min);
    EXPECT_EQ(Value(2147483648.0).getInteger(), max);
    EXPECT_EQ(Value(-2147483649.0).getInteger(), min);
    EXPECT_EQ(Value(std::numeric_limits<double>::infinity()).getInteger(), max);
    EXPECT_EQ(Value(-std::numeric_limits<double>::infinity()).getInteger(), min);
    EXPECT_EQ(Value(std::nan("")).tryGetInteger(7), 7);
    EXPECT_EQ(Value(std::nan("")).getInteger(), 0);
}

TEST(ValueInteger, ADoubleInRangeIsTruncatedAsBefore){
    EXPECT_EQ(Value(3.99).getInteger(), 3);
    EXPECT_EQ(Value(-3.99).getInteger(), -3);
    EXPECT_EQ(Value(2147483647.9).getInteger(), 2147483647);
    EXPECT_EQ(Value(-2147483648.9).getInteger(), std::numeric_limits<int>::min());
    EXPECT_EQ(Value(41).getInteger(), 41);
    EXPECT_EQ(Value("x").tryGetInteger(9), 9);
}
