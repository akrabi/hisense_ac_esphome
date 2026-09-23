#include "test_support.h"
#include <algorithm>
#include <cstring>

void framing() {
    const auto captured = capture();
    for (size_t split = 0; split <= captured.size(); ++split) {
        protocol::FrameParser parser;
        uint32_t now = 0;
        auto count = feed(parser, Bytes(captured.begin(), captured.begin() + split), now);
        now += 20;
        count += feed(parser, Bytes(captured.begin() + split, captured.end()), now);
        CHECK(count == 1);
        CHECK(std::memcmp(parser.data(), captured.data(), captured.size()) == 0);
    }
    protocol::FrameParser parser;
    uint32_t now = 0;
    CHECK(feed(parser, {0x00, 0xFF, 0xF5, 0xFB, 0xF4, 0xF4}, now) == 0);
    CHECK(feed(parser, captured, now) == 1);
    CHECK(feed(parser, captured, now) == 1);
    CHECK(feed(parser, captured, now) == 1);

    // Every truncation of the capture must recover on a fresh header.
    for (size_t end = 1; end < captured.size(); ++end) {
        parser.reset();
        CHECK(feed(parser, Bytes(captured.begin(), captured.begin() + end), now) == 0);
        CHECK(feed(parser, captured, now) == 1);
    }

    auto escaped = synthetic();
    escaped[16] = 0x80;
    escaped[21] = 0xF4;
    escaped[44] = 0xFF;
    escaped[77] = 0x7F;  // Last payload byte must participate in checksum.
    seal(escaped);
    CHECK(feed(parser, wire(escaped), now) == 1);
    CHECK(std::memcmp(parser.data(), escaped.data(), escaped.size()) == 0);

    auto checksum_escape = synthetic();
    unsigned sum = checksum_escape[78] * 256 + checksum_escape[79];
    checksum_escape[77] = static_cast<uint8_t>(0xF4 - (sum & 0xFF));
    seal(checksum_escape);
    CHECK(checksum_escape[79] == 0xF4);
    CHECK(wire(checksum_escape).size() == checksum_escape.size() + 1);
    CHECK(feed(parser, wire(checksum_escape), now) == 1);
    CHECK(std::memcmp(parser.data(), checksum_escape.data(), checksum_escape.size()) == 0);
}

void malformed_and_bounds() {
    protocol::FrameParser parser;
    uint32_t now = 0;
    const auto good = capture();
    for (size_t index : {size_t(2), size_t(3), size_t(18), size_t(78), size_t(79), size_t(80), size_t(81)}) {
        auto bad = good;
        bad[index] ^= 1;
        CHECK(feed(parser, wire(bad), now) == 0);
        CHECK(feed(parser, good, now) == 1);
    }
    // Framing is bounded separately from the single supported status layout.
    for (size_t length = 9; length <= protocol::MAX_FRAME_SIZE; ++length) {
        const auto frame = synthetic(length);
        CHECK(feed(parser, wire(frame), now) == 1);
        DeviceStatus status;
        CHECK(protocol::decode_status(parser.data(), length, status) == (length == 82));
    }
    for (size_t length = protocol::MAX_FRAME_SIZE + 1; length <= 264; ++length) {
        CHECK(feed(parser, wire(synthetic(length)), now) == 0);
        CHECK(feed(parser, good, now) == 1);
    }
    CHECK(protocol::MAX_FRAME_SIZE == 128);
    auto maximum = synthetic(protocol::MAX_FRAME_SIZE);
    std::fill(maximum.begin() + 5, maximum.end() - 4, 0xF4);
    seal(maximum);
    CHECK(feed(parser, wire(maximum), now) == 1);
    CHECK(std::memcmp(parser.data(), maximum.data(), maximum.size()) == 0);

    // Extra data must not overflow a full-capacity frame or overwrite status.
    auto oversized = synthetic(protocol::MAX_FRAME_SIZE);
    oversized.insert(oversized.end() - 4, 0);
    CHECK(oversized.size() == protocol::MAX_FRAME_SIZE + 1);
    CHECK(feed(parser, wire(oversized), now) == 0);
    CHECK(feed(parser, good, now) == 1);
    DeviceStatus unchanged;
    unchanged.indoor_temperature_setting = 23;
    CHECK(!protocol::decode_status(oversized.data(), oversized.size(), unchanged));
    CHECK(unchanged.indoor_temperature_setting == 23);
    for (uint8_t length : {uint8_t(0), uint8_t(1), uint8_t(254), uint8_t(255)}) {
        auto wrong_length = good;
        wrong_length[4] = length;
        CHECK(feed(parser, wire(wrong_length), now) == 0);
        CHECK(feed(parser, good, now) == 1);
    }

    // An escape must be doubled, never skip an arbitrary following byte.
    CHECK(feed(parser, {0xF4, 0xF5, 1, 0x40, 0x49, 0xF4, 0x12}, now) == 0);
    CHECK(feed(parser, good, now) == 1);
    // An F4 checksum byte without stuffing cannot consume the footer as data.
    auto invalid_checksum = synthetic();
    invalid_checksum[78] = 0xF4;
    CHECK(feed(parser, wire(invalid_checksum), now) == 0);
    CHECK(feed(parser, good, now) == 1);
}

void public_issue_captures() {
    const auto escaped = capture("issue_1_status_82_escaped.hex");
    CHECK(escaped.size() == 83 && escaped[50] == 0xF4 && escaped[51] == 0xF4);
    Bytes unescaped = escaped;
    unescaped.erase(unescaped.begin() + 51);
    CHECK(unescaped.size() == 82);
    for (const auto &packet : {escaped, capture()}) {
        const auto &expected = packet.size() == 83 ? unescaped : packet;
        for (size_t split = 0; split <= packet.size(); ++split) {
            protocol::FrameParser parser;
            uint32_t now = 0;
            auto count = feed(parser, Bytes(packet.begin(), packet.begin() + split), now);
            now += 20;
            count += feed(parser, Bytes(packet.begin() + split, packet.end()), now);
            CHECK(count == 1);
            CHECK(std::memcmp(parser.data(), expected.data(), expected.size()) == 0);
            DeviceStatus status;
            CHECK(protocol::decode_status(parser.data(), expected.size(), status));
            if (packet.size() == 83) {
                CHECK(status.mode_status == 3);
                CHECK(status.indoor_temperature_setting == 19);
                CHECK(status.indoor_temperature_status == 23);
                CHECK(status.outdoor_temperature == 21);
                CHECK(status.compressor_frequency_send == 0);
            }
        }
    }
    protocol::FrameParser parser;
    uint32_t now = 0;
    Bytes concatenated = escaped;
    concatenated.insert(concatenated.end(), escaped.begin(), escaped.end());
    CHECK(feed(parser, concatenated, now) == 2);
    // Valid framing in an issue report does not establish its status mapping.
    const auto unsupported = capture("issue_6_status_160.hex");
    CHECK(unsupported.size() == 160);
    CHECK(feed(parser, unsupported, now) == 0);
    DeviceStatus status;
    status.indoor_temperature_setting = 23;
    CHECK(!protocol::decode_status(unsupported.data(), unsupported.size(), status));
    CHECK(status.indoor_temperature_setting == 23);
    CHECK(feed(parser, escaped, now) == 1);
}

void checksum_end_boundary() {
    protocol::FrameParser parser;
    uint32_t now = 0;
    for (size_t size : {size_t(82), protocol::MAX_FRAME_SIZE}) {
        auto frame = synthetic(size);
        frame[size - 5] = 0x7F;
        seal(frame);
        CHECK(feed(parser, wire(frame), now) == 1);
        CHECK(std::memcmp(parser.data(), frame.data(), size) == 0);
        // The historical checksum loop excluded the last payload byte.
        unsigned old_sum = frame[size - 4] * 256 + frame[size - 3] - 0x7F;
        frame[size - 4] = static_cast<uint8_t>(old_sum >> 8);
        frame[size - 3] = static_cast<uint8_t>(old_sum);
        CHECK(feed(parser, wire(frame), now) == 0);
        CHECK(feed(parser, capture(), now) == 1);
    }
}

void control_response_captures() {
    for (const char *name : {"cool_control_65.hex", "cool_poll_66.hex",
                             "dry_control_65.hex", "dry_poll_66.hex"}) {
        const auto decoded = capture(name);
        const auto packet = wire(decoded);
        CHECK(decoded.size() == 82);
        for (size_t split = 0; split <= packet.size(); ++split) {
            protocol::FrameParser parser;
            uint32_t now = 0;
            auto count = feed(parser, Bytes(packet.begin(), packet.begin() + split), now);
            now += 20;
            count += feed(parser, Bytes(packet.begin() + split, packet.end()), now);
            CHECK(count == 1 && parser.invalid_frames() == 0);
            CHECK(std::memcmp(parser.data(), decoded.data(), decoded.size()) == 0);
            DeviceStatus status;
            status.indoor_temperature_setting = 22;
            const bool polled = decoded[13] == 0x66;
            CHECK(protocol::decode_status(parser.data(), decoded.size(), status) == polled);
            CHECK(status.indoor_temperature_setting == (polled ? 27 : 22));
        }
    }
}

void timeouts_and_instances() {
    protocol::FrameParser first, second;
    const auto a = wire(capture());
    auto b_decoded = synthetic();
    b_decoded[21] = 0xF4;
    seal(b_decoded);
    const auto b = wire(b_decoded);
    uint32_t now = 0;
    unsigned count_a = 0, count_b = 0;
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
        if (i < a.size()) count_a += first.feed(a[i], now) != 0;
        if (i < b.size()) count_b += second.feed(b[i], now) != 0;
        ++now;
    }
    CHECK(count_a == 1 && count_b == 1);
    CHECK(std::memcmp(first.data(), a.data(), a.size()) == 0);
    CHECK(std::memcmp(second.data(), b_decoded.data(), b_decoded.size()) == 0);

    // Timeouts and explicit reset clear an outstanding escape as well.
    for (bool explicit_reset : {false, true}) {
        CHECK(feed(first, {0xF4, 0xF5, 1, 0x40, 0x49, 0xF4}, now) == 0);
        if (explicit_reset) first.reset();
        else {
            now += protocol::INTER_BYTE_TIMEOUT_MS;
            first.expire(now);
        }
        CHECK(feed(first, a, now) == 1);
    }
    // Unsigned elapsed time works across millis() wrap, including expiration.
    now = UINT32_MAX - 30;
    CHECK(feed(first, a, now) == 1);
    now = UINT32_MAX - 5;
    CHECK(feed(first, {0xF4, 0xF5, 1}, now) == 0);
    now += protocol::INTER_BYTE_TIMEOUT_MS;
    CHECK(feed(first, a, now) == 1);

    first.reset();
    CHECK(first.feed(0xF4, 0) == 0);
    CHECK(first.feed(0xF5, 99) == 0);
    now = 100;
    CHECK(feed(first, Bytes(a.begin() + 2, a.end()), now) == 1);
    CHECK(first.feed(0xF4, now) == 0);
    CHECK(first.feed(0xF5, now + 100) == 0);
    now += 101;
    CHECK(feed(first, Bytes(a.begin() + 2, a.end()), now) == 0);
    CHECK(feed(first, a, now) == 1);
}

void decoding() {
    auto frame = synthetic();
    frame[16] = 18;
    frame[18] = 0x2C;
    frame[19] = 23;
    frame[20] = 25;
    frame[21] = 244;
    frame[22] = 0x80;
    frame[23] = 0xFF;
    frame[35] = 0xC0;
    frame[37] = 0x80;
    frame[41] = 55;
    frame[42] = 56;
    frame[43] = 57;
    frame[44] = 0xF6;
    frame[45] = 0x80;
    frame[46] = 0x7F;
    frame[47] = 0xFF;
    seal(frame);
    DeviceStatus status;
    CHECK(protocol::decode_status(frame.data(), frame.size(), status));
    CHECK(status.wind_status == 18 && status.run_status == 3 && status.mode_status == 2);
    CHECK(status.indoor_temperature_setting == 23 && status.indoor_temperature_status == 25);
    CHECK(status.indoor_pipe_temperature == 244);
    CHECK(status.indoor_humidity_setting == -128 && status.indoor_humidity_status == -1);
    CHECK(status.left_right && status.up_down && status.back_led);
    CHECK(status.compressor_frequency == 55 && status.compressor_frequency_setting == 56);
    CHECK(status.compressor_frequency_send == 57);
    CHECK(status.outdoor_temperature == -10 && status.outdoor_condenser_temperature == -128);
    CHECK(status.compressor_exhaust_temperature == 127 && status.target_exhaust_temperature == -1);
    for (unsigned bits = 0; bits <= 255; ++bits) {
        frame[18] = frame[35] = frame[37] = static_cast<uint8_t>(bits);
        seal(frame);
        CHECK(protocol::decode_status(frame.data(), frame.size(), status));
        CHECK(status.run_status == ((bits >> 2) & 3) && status.mode_status == (bits >> 4));
        CHECK(status.left_right == ((bits & 0x40) != 0));
        CHECK(status.up_down == ((bits & 0x80) != 0));
        CHECK(status.back_led == ((bits & 0x80) != 0));
    }
    // Unknown class must not update even one field in the retained snapshot.
    frame[13] = 0x65;
    frame[19] = 30;
    seal(frame);
    CHECK(!protocol::decode_status(frame.data(), frame.size(), status));
    CHECK(status.indoor_temperature_setting == 23);
    CHECK(!protocol::decode_status(nullptr, 82, status));
    CHECK(!protocol::decode_status(frame.data(), 48, status));
    CHECK(!protocol::decode_status(frame.data(), 0, status));
    frame[13] = 0x66;
    seal(frame);
    frame[79] ^= 1;
    CHECK(!protocol::decode_status(frame.data(), frame.size(), status));
    CHECK(status.indoor_temperature_setting == 23);
    const auto real = capture();
    CHECK(protocol::decode_status(real.data(), real.size(), status));
    CHECK(status.indoor_temperature_setting == 21 && status.indoor_temperature_status == 23);
}

void deterministic_noise() {
    protocol::FrameParser parser;
    uint32_t now = 0, random = 1234567;
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525U + 1013904223U;
        const size_t size = parser.feed(static_cast<uint8_t>(random >> 24), now++);
        CHECK(size <= protocol::MAX_FRAME_SIZE);
        if ((i % 1000) == 0) {
            now += protocol::INTER_BYTE_TIMEOUT_MS;
            CHECK(feed(parser, capture(), now) == 1);
        }
    }
}

int main() {
    framing();
    malformed_and_bounds();
    public_issue_captures();
    control_response_captures();
    checksum_end_boundary();
    timeouts_and_instances();
    decoding();
    deterministic_noise();
    return 0;
}
