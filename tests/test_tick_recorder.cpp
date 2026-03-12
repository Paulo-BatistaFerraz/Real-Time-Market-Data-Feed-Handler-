#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "qf/data/data_types.hpp"
#include "qf/data/recorder/binary_writer.hpp"
#include "qf/data/storage/tick_store.hpp"
#include "qf/common/types.hpp"

using namespace qf;
using namespace qf::data;

namespace {

Symbol make_symbol(const char* name) {
    Symbol s{};
    for (int i = 0; i < 8 && name[i]; ++i) s.data[i] = name[i];
    return s;
}

TickRecord make_tick(Timestamp ts, const char* sym, char type,
                     const uint8_t* payload, uint16_t len) {
    TickRecord r;
    r.timestamp = ts;
    r.symbol = make_symbol(sym);
    r.message_type = type;
    r.payload_length = len;
    std::memcpy(r.payload, payload, len);
    return r;
}

const std::string TEST_FILE = "test_tick_roundtrip.qfbt";

struct TickRecorderTest : ::testing::Test {
    void TearDown() override {
        std::remove(TEST_FILE.c_str());
    }
};

}  // namespace

// --- Binary write → read round-trip preserves tick data ---

TEST_F(TickRecorderTest, SingleRecordRoundTrip) {
    uint8_t payload[] = {0x01, 0x02, 0xAA, 0xBB, 0xCC};
    TickRecord original = make_tick(1'000'000'000ULL, "AAPL", 'A', payload, 5);

    // Write one record.
    {
        BinaryWriter writer(TEST_FILE, 1);
        writer.write(original);
        writer.close();
        EXPECT_EQ(writer.record_count(), 1u);
    }

    // Read it back via TickStore.
    {
        TickStore store(TEST_FILE);
        ASSERT_TRUE(store.is_indexed());
        EXPECT_EQ(store.record_count(), 1u);

        auto ticks = store.read_range(0, UINT64_MAX);
        ASSERT_EQ(ticks.size(), 1u);

        const auto& t = ticks[0];
        EXPECT_EQ(t.timestamp, original.timestamp);
        EXPECT_EQ(t.symbol, original.symbol);
        EXPECT_EQ(t.message_type, original.message_type);
        EXPECT_EQ(t.payload_length, original.payload_length);
        EXPECT_EQ(std::memcmp(t.payload, original.payload, original.payload_length), 0);
    }
}

TEST_F(TickRecorderTest, MultipleRecordRoundTrip) {
    constexpr int N = 100;
    std::vector<TickRecord> originals;
    originals.reserve(N);

    for (int i = 0; i < N; ++i) {
        uint8_t payload[4];
        payload[0] = static_cast<uint8_t>(i & 0xFF);
        payload[1] = static_cast<uint8_t>((i >> 8) & 0xFF);
        payload[2] = static_cast<uint8_t>(i * 3);
        payload[3] = static_cast<uint8_t>(i * 7);
        originals.push_back(make_tick(
            static_cast<Timestamp>(i * 1'000'000ULL),
            (i % 2 == 0) ? "AAPL" : "MSFT",
            (i % 3 == 0) ? 'A' : 'T',
            payload, 4));
    }

    // Write all records.
    {
        BinaryWriter writer(TEST_FILE, 16);
        for (const auto& rec : originals) {
            writer.write(rec);
        }
        writer.close();
        EXPECT_EQ(writer.record_count(), static_cast<uint64_t>(N));
    }

    // Read all back.
    {
        TickStore store(TEST_FILE);
        ASSERT_TRUE(store.is_indexed());
        EXPECT_EQ(store.record_count(), static_cast<size_t>(N));

        auto ticks = store.read_range(0, UINT64_MAX);
        ASSERT_EQ(ticks.size(), static_cast<size_t>(N));

        for (int i = 0; i < N; ++i) {
            EXPECT_EQ(ticks[i].timestamp, originals[i].timestamp) << "record " << i;
            EXPECT_EQ(ticks[i].symbol, originals[i].symbol) << "record " << i;
            EXPECT_EQ(ticks[i].message_type, originals[i].message_type) << "record " << i;
            EXPECT_EQ(ticks[i].payload_length, originals[i].payload_length) << "record " << i;
            EXPECT_EQ(std::memcmp(ticks[i].payload, originals[i].payload,
                                  originals[i].payload_length), 0)
                << "record " << i;
        }
    }
}

TEST_F(TickRecorderTest, HeaderValidation) {
    uint8_t payload[] = {0x42};
    TickRecord rec = make_tick(500, "GOOG", 'E', payload, 1);

    {
        BinaryWriter writer(TEST_FILE, 1);
        writer.write(rec);
        writer.close();
    }

    // Read the raw header to verify magic and record count.
    FILE* f = std::fopen(TEST_FILE.c_str(), "rb");
    ASSERT_NE(f, nullptr);

    FileHeader hdr;
    EXPECT_EQ(std::fread(&hdr, sizeof(FileHeader), 1, f), 1u);
    EXPECT_EQ(hdr.magic, BINARY_FILE_MAGIC);
    EXPECT_EQ(hdr.version, BINARY_FILE_VERSION);
    EXPECT_EQ(hdr.record_count, 1u);

    std::fclose(f);
}

TEST_F(TickRecorderTest, MaxPayloadPreserved) {
    uint8_t payload[MAX_PAYLOAD_SIZE];
    for (size_t i = 0; i < MAX_PAYLOAD_SIZE; ++i) {
        payload[i] = static_cast<uint8_t>(i);
    }
    TickRecord original = make_tick(999, "TSLA", 'R', payload, MAX_PAYLOAD_SIZE);

    {
        BinaryWriter writer(TEST_FILE, 1);
        writer.write(original);
        writer.close();
    }

    TickStore store(TEST_FILE);
    auto ticks = store.read_range(0, UINT64_MAX);
    ASSERT_EQ(ticks.size(), 1u);

    EXPECT_EQ(ticks[0].payload_length, static_cast<uint16_t>(MAX_PAYLOAD_SIZE));
    EXPECT_EQ(std::memcmp(ticks[0].payload, payload, MAX_PAYLOAD_SIZE), 0);
}

TEST_F(TickRecorderTest, SymbolFilteredRead) {
    uint8_t payload[] = {0x01};
    std::vector<TickRecord> records;
    records.push_back(make_tick(100, "AAPL", 'A', payload, 1));
    records.push_back(make_tick(200, "MSFT", 'A', payload, 1));
    records.push_back(make_tick(300, "AAPL", 'T', payload, 1));
    records.push_back(make_tick(400, "GOOG", 'T', payload, 1));
    records.push_back(make_tick(500, "AAPL", 'X', payload, 1));

    {
        BinaryWriter writer(TEST_FILE, 8);
        for (const auto& r : records) writer.write(r);
        writer.close();
    }

    TickStore store(TEST_FILE);
    auto aapl_ticks = store.read_range(make_symbol("AAPL"), 0, UINT64_MAX);
    EXPECT_EQ(aapl_ticks.size(), 3u);
    for (const auto& t : aapl_ticks) {
        EXPECT_EQ(t.symbol, make_symbol("AAPL"));
    }

    auto msft_ticks = store.read_range(make_symbol("MSFT"), 0, UINT64_MAX);
    EXPECT_EQ(msft_ticks.size(), 1u);
}
