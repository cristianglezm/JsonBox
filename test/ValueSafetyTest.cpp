#include <JsonBox.h>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <exception>
#include <cstdlib>
#include <functional>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

    // Can an allocation made by the library (not only by this executable) be made to fail? A replaced
    // operator new reaches the library when it is linked in statically and, on most systems, when it is a
    // shared library, but not on all of them (a DLL has its own allocator, for one).
    bool injectionWorks(){
        bool threw = false;
        try{
            // through a volatile pointer, or an optimizer may drop the allocation altogether
            allocations = 0;
            failAt = 0;
            int* volatile probe = new int(1);
            delete probe;
        }catch(const std::bad_alloc&){
            threw = true;
        }
        failAt = -1;
        if(!threw){
            return false;
        }
        // the first allocation of a string copy is made inside the library
        const std::string text(200, 'x');
        Value value;
        threw = false;
        try{
            allocations = 0;
            failAt = 0;
            value.setString(text);
        }catch(const std::bad_alloc&){
            threw = true;
        }
        failAt = -1;
        return threw;
    }

    // Where the injection is, for the message of a std::terminate: a noexcept function that does not let
    // bad_alloc through ends the program without a word, and then this is all there is to read.
    const char* currentCase = "";
    long currentPoint = -1;

    void reportTerminate(){
        std::fprintf(stderr, "\nstd::terminate while failing allocation %ld of '%s': a noexcept function did not let "
                     "bad_alloc through (the debug library of MSVC allocates in constructors it declares noexcept)\n",
                     currentPoint, currentCase);
        std::fflush(stderr);
        std::abort();
    }

    struct TerminateReport{
        std::terminate_handler previous;
        TerminateReport() : previous(std::set_terminate(reportTerminate)){}
        ~TerminateReport(){ std::set_terminate(previous); }
    };

    // Runs `operation` on a copy of `original`, failing the 0th, 1st, 2nd... allocation inside it,
    // until it finishes. A failed operation must leave the value as it was (and destroying it must
    // not touch freed memory: that is what the sanitizers are for). Returns the failures injected.
    // (mustBeUnchanged = false only checks that the value is still valid: it is destroyed below)
    int forEveryFailurePoint(const Value& original, const std::function<void(Value&)>& operation,
                             bool mustBeUnchanged = true){
        const long limit = 512; // the debug library of some compilers allocates for every container
        int failures = 0;
        bool completed = false;
        for(long point = 0; point < limit && !completed; ++point){
            Value value = original;
            allocations = 0;
            currentPoint = point;
            failAt = point;
            try{
                operation(value);
                completed = true;
            }catch(const std::bad_alloc&){
                ++failures;
            }
            const long made = allocations;
            failAt = -1;
            if(!completed){
                EXPECT_TRUE(!mustBeUnchanged || value == original)
                    << "allocation " << point << " failed (the operation made " << made
                    << " before it) and the value changed";
            }
        }
        EXPECT_TRUE(completed) << "the operation did not finish within " << limit << " failure points";
        return failures;
    }

    struct Injection{
        const char* name;
        Value original;
        std::function<void(Value&)> operation;
        bool mustBeUnchanged;
    };

    // every case on its own, with its name in every message
    void expectEveryFailureHandled(const std::vector<Injection>& cases){
        TerminateReport report;
        for(const auto& c : cases){
            SCOPED_TRACE(c.name);
            currentCase = c.name;
            const int failures = forEveryFailurePoint(c.original, c.operation, c.mustBeUnchanged);
            EXPECT_GT(failures, 0) << c.name << ": no allocation inside the operation could be made to fail";
        }
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
    expectEveryFailureHandled({
        {"copy a document over a string", longString(), [&](Value& v){ v = source; }, true},
        {"copy a document over a document", sample(), [&](Value& v){ v = source; }, true},
        {"copy a document over an integer", Value(1), [&](Value& v){ v = source; }, true}
    });
}

TEST(ValueExceptionSafety, SettersLeaveTheValueAloneWhenAnAllocationFails){
    if(!injectionWorks()){ GTEST_SKIP() << "operator new cannot be replaced in this build"; }
    Object object;
    object["k"] = Value(std::string(100, 'k'));
    Array array;
    array.push_back(Value(std::string(100, 'a')));
    const std::string text(100, 't');

    expectEveryFailureHandled({
        {"setString on a document", sample(), [&](Value& v){ v.setString(text); }, true},
        {"setInteger on a document", sample(), [&](Value& v){ v.setInteger(1); }, true},
        {"setDouble on a document", sample(), [&](Value& v){ v.setDouble(1.5); }, true},
        {"setBoolean on a document", sample(), [&](Value& v){ v.setBoolean(true); }, true},
        {"setObject on a string", longString(), [&](Value& v){ v.setObject(object); }, true},
        {"setArray on a string", longString(), [&](Value& v){ v.setArray(array); }, true},
        {"assign a std::string to a document", sample(), [&](Value& v){ v = text; }, true},
        // the value becomes an object before the key is inserted: valid, not necessarily unchanged
        {"v[\"key\"] = 1 on a string", longString(), [&](Value& v){ v["key"] = 1; }, false},
        {"v[3] on a string", longString(), [&](Value& v){ (void) v[3]; }, true}
    });
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

TEST(ValueIndex, UsingAnIndexOfAnExistingElementAllocatesNothing){
    // The check of the index runs for every v[i]. It must not allocate: the debug library of MSVC allocates
    // in the constructor of a vector, which is noexcept, so a check that makes one ends the program when
    // an allocation fails, where it should not even notice. (Not a rejected index: the exception that is
    // thrown for it allocates its message.)
    if(!injectionWorks()){ GTEST_SKIP() << "operator new cannot be replaced in this build"; }
    TerminateReport report;
    currentCase = "v[i] of an existing element";
    Value array;
    array[2] = 7;
    for(std::size_t index : {std::size_t(0), std::size_t(1), std::size_t(2)}){
        bool allocated = false;
        int found = -1;
        allocations = 0;
        currentPoint = 0;
        failAt = 0; // the first allocation made from here on fails
        try{
            found = array[index].getInteger();
        }catch(const std::bad_alloc&){
            allocated = true;
        }
        failAt = -1;
        EXPECT_FALSE(allocated) << "the check of the index allocated (index " << index << ")";
        EXPECT_EQ(found, index == 2 ? 7 : 0);
    }
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

TEST(ValueConstruction, ParsingThatFailsInTheStreamConstructorDoesNotLeak){
    // the exception leaves the constructor, so the destructor never runs: the tree read so far
    // has to be freed by the constructor (LeakSanitizer reports it when the program ends)
    std::stringstream malformed("[\"a long enough string to not be stored inline in the object\", [1, 2], nul, 1]");
    EXPECT_THROW(JsonBox::Value parsed(malformed), std::exception);
    std::stringstream cut("{\"key\": [\"a long enough string to not be stored inline in the object\"");
    JsonBox::Value ignored;
    try{
        JsonBox::Value parsed(cut);
    }catch(const std::exception&){
    }
    SUCCEED();
}
