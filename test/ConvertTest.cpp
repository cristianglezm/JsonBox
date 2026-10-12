#include <JsonBox/Convert.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

using JsonBox::Convert;
using JsonBox::String32;

namespace{
    // A copy whose heap block ends right after its terminating NUL: reading two bytes past the
    // last character is a heap-buffer-overflow under AddressSanitizer. The 64 characters in front
    // keep it out of the small-string buffer.
    std::string tight(const std::string& tail){
        std::string s(64, 'a');
        s += tail;
        return std::string(s.data(), s.size());
    }
}

TEST(ConvertDecode, ValidTextRoundTrips){
    const std::string text = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    auto decoded = Convert::decodeUTF8(text);
    ASSERT_EQ(decoded.size(), 4u);
    EXPECT_EQ(decoded[1], 0xE9);
    EXPECT_EQ(decoded[2], 0x20AC);
    EXPECT_EQ(decoded[3], 0x1F600);
    EXPECT_EQ(Convert::encodeToUTF8(decoded), text);
}

TEST(ConvertDecode, ATruncatedSequenceAtTheEndIsOneReplacementCharacter){
    // every proper prefix of a 2, 3, 4, 5 and 6 byte sequence
    const std::vector<std::string> sequences = {
        "\xC3\xA9", "\xE2\x82\xAC", "\xF0\x9F\x98\x80",
        "\xF8\x88\x80\x80\x80", "\xFC\x84\x80\x80\x80\x80"
    };
    for(const auto& sequence : sequences){
        for(std::size_t length = 1; length < sequence.size(); ++length){
            auto decoded = Convert::decodeUTF8(tight(sequence.substr(0, length)));
            ASSERT_EQ(decoded.size(), 65u) << "prefix of " << sequence.size() << " bytes, length " << length;
            EXPECT_EQ(decoded.back(), 0xFFFD);
            EXPECT_EQ(decoded[63], 'a');
        }
    }
}

TEST(ConvertDecode, ACompleteSequenceAtTheEndIsStillDecoded){
    auto decoded = Convert::decodeUTF8(tight("\xF0\x9F\x98\x80"));
    ASSERT_EQ(decoded.size(), 65u);
    EXPECT_EQ(decoded.back(), 0x1F600);
}

TEST(ConvertDecode, ADamagedSequenceIsStillAcceptedAsBefore){
    // not a continuation byte: the decoder has always been lenient here, and still is
    EXPECT_EQ(Convert::decodeUTF8("\xC3(").size(), 1u);
}

TEST(ConvertEncode, EveryValueTheDecoderCanProduceRoundTrips){
    const std::vector<std::pair<int32_t, std::size_t>> values = {
        {0x7F, 1}, {0x80, 2}, {0x7FF, 2}, {0x800, 3}, {0xFFFF, 3}, {0x10000, 4}, {0x10FFFF, 4},
        {0x1FFFFF, 4}, {0x200000, 5}, {0x3FFFFFF, 5},
        // the 6 byte form: the branch ended at 0x8000000 instead of 0x80000000 and wrote 5 bytes
        {0x4000000, 6}, {0x7FFFFFF, 6}, {0x8000000, 6}, {0x7FFFFFFF, 6}
    };
    for(const auto& value : values){
        String32 in{value.first};
        auto text = Convert::encodeToUTF8(in);
        EXPECT_EQ(text.size(), value.second) << std::hex << value.first;
        EXPECT_EQ(Convert::decodeUTF8(text), in) << std::hex << value.first;
    }
}
