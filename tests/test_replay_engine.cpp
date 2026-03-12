#include <gtest/gtest.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "qf/data/data_types.hpp"
#include "qf/data/recorder/binary_writer.hpp"
#include "qf/data/storage/tick_store.hpp"
#include "qf/data/replay/replay_clock.hpp"
#include "qf/data/replay/replay_engine.hpp"
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

const std::string TEST_FILE = "test_replay_engine.qfbt";

struct ReplayEngineTest : ::testing::Test {
    void TearDown() override {
        std::remove(TEST_FILE.c_str());
    }
};

}  // namespace

// --- Replay engine emits events in timestamp order ---

TEST_F(ReplayEngineTest, EmitsInTimestampOrder) {
    // Write ticks from multiple symbols, interleaved but in timestamp order.
    uint8_t p1[] = {0x01};
    uint8_t p2[] = {0x02};
    uint8_t p3[] = {0x03};
    uint8_t p4[] = {0x04};
    uint8_t p5[] = {0x05};

    {
        BinaryWriter writer(TEST_FILE, 8);
        writer.write(make_tick(100, "MSFT", 'T', p1, 1));
        writer.write(make_tick(300, "AAPL", 'E', p2, 1));
        writer.write(make_tick(500, "AAPL", 'A', p3, 1));
        writer.write(make_tick(700, "MSFT", 'A', p4, 1));
        writer.write(make_tick(900, "GOOG", 'X', p5, 1));
        writer.close();
    }

    // Set up replay with as-fast-as-possible mode.
    TickStore store(TEST_FILE);
    ReplayClock clock;
    clock.set_speed(0.0);

    std::vector<uint64_t> received_timestamps;
    std::vector<uint8_t> received_payloads;

    ReplayEngine engine(store, clock, [&](const uint8_t* data, size_t len, uint64_t ts) {
        received_timestamps.push_back(ts);
        if (len > 0) received_payloads.push_back(data[0]);
    });

    engine.load(0, UINT64_MAX);
    engine.start();

    // All 5 ticks should be delivered.
    ASSERT_EQ(received_timestamps.size(), 5u);
    EXPECT_EQ(engine.ticks_published(), 5u);

    // Timestamps must be monotonically non-decreasing.
    for (size_t i = 1; i < received_timestamps.size(); ++i) {
        EXPECT_LE(received_timestamps[i - 1], received_timestamps[i])
            << "timestamp " << i << " is out of order";
    }

    // Verify exact order: 100, 300, 500, 700, 900
    EXPECT_EQ(received_timestamps[0], 100u);
    EXPECT_EQ(received_timestamps[1], 300u);
    EXPECT_EQ(received_timestamps[2], 500u);
    EXPECT_EQ(received_timestamps[3], 700u);
    EXPECT_EQ(received_timestamps[4], 900u);

    // Verify payloads match the timestamp order.
    EXPECT_EQ(received_payloads[0], 0x01);  // ts=100
    EXPECT_EQ(received_payloads[1], 0x02);  // ts=300
    EXPECT_EQ(received_payloads[2], 0x03);  // ts=500
    EXPECT_EQ(received_payloads[3], 0x04);  // ts=700
    EXPECT_EQ(received_payloads[4], 0x05);  // ts=900
}

TEST_F(ReplayEngineTest, SymbolFilteredReplay) {
    uint8_t p[] = {0xAA};

    {
        BinaryWriter writer(TEST_FILE, 8);
        writer.write(make_tick(100, "AAPL", 'A', p, 1));
        writer.write(make_tick(200, "MSFT", 'A', p, 1));
        writer.write(make_tick(300, "AAPL", 'T', p, 1));
        writer.write(make_tick(400, "GOOG", 'T', p, 1));
        writer.close();
    }

    TickStore store(TEST_FILE);
    ReplayClock clock;
    clock.set_speed(0.0);

    std::vector<uint64_t> received_timestamps;
    ReplayEngine engine(store, clock, [&](const uint8_t*, size_t, uint64_t ts) {
        received_timestamps.push_back(ts);
    });

    engine.load(make_symbol("AAPL"), 0, UINT64_MAX);
    engine.start();

    ASSERT_EQ(received_timestamps.size(), 2u);
    EXPECT_EQ(received_timestamps[0], 100u);
    EXPECT_EQ(received_timestamps[1], 300u);
}

TEST_F(ReplayEngineTest, TimeRangeFilteredReplay) {
    uint8_t p[] = {0x01};

    {
        BinaryWriter writer(TEST_FILE, 8);
        writer.write(make_tick(100, "AAPL", 'A', p, 1));
        writer.write(make_tick(200, "AAPL", 'A', p, 1));
        writer.write(make_tick(300, "AAPL", 'A', p, 1));
        writer.write(make_tick(400, "AAPL", 'A', p, 1));
        writer.write(make_tick(500, "AAPL", 'A', p, 1));
        writer.close();
    }

    TickStore store(TEST_FILE);
    ReplayClock clock;
    clock.set_speed(0.0);

    std::vector<uint64_t> received_timestamps;
    ReplayEngine engine(store, clock, [&](const uint8_t*, size_t, uint64_t ts) {
        received_timestamps.push_back(ts);
    });

    engine.load(200, 400);
    engine.start();

    ASSERT_EQ(received_timestamps.size(), 3u);
    EXPECT_EQ(received_timestamps[0], 200u);
    EXPECT_EQ(received_timestamps[1], 300u);
    EXPECT_EQ(received_timestamps[2], 400u);
}

TEST_F(ReplayEngineTest, EmptyStoreProducesNoEvents) {
    // Create an empty file.
    {
        BinaryWriter writer(TEST_FILE, 1);
        writer.close();
    }

    TickStore store(TEST_FILE);
    ReplayClock clock;
    clock.set_speed(0.0);

    size_t call_count = 0;
    ReplayEngine engine(store, clock, [&](const uint8_t*, size_t, uint64_t) {
        ++call_count;
    });

    engine.load(0, UINT64_MAX);
    engine.start();

    EXPECT_EQ(call_count, 0u);
    EXPECT_EQ(engine.ticks_published(), 0u);
}

TEST_F(ReplayEngineTest, BytesPublishedAccurate) {
    uint8_t p3[] = {0x01, 0x02, 0x03};
    uint8_t p5[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    {
        BinaryWriter writer(TEST_FILE, 8);
        writer.write(make_tick(100, "AAPL", 'A', p3, 3));
        writer.write(make_tick(200, "AAPL", 'A', p5, 5));
        writer.close();
    }

    TickStore store(TEST_FILE);
    ReplayClock clock;
    clock.set_speed(0.0);

    ReplayEngine engine(store, clock, [](const uint8_t*, size_t, uint64_t) {});

    engine.load(0, UINT64_MAX);
    engine.start();

    EXPECT_EQ(engine.ticks_published(), 2u);
    EXPECT_EQ(engine.bytes_published(), 8u);  // 3 + 5
}
