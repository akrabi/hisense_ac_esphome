#include "test_support.h"

static uint32_t clock_ms = 0;
static uint32_t millis() { return clock_ms; }
#define ESP_LOGD(...) ((void)0)

// The adapter body is extracted at configure time from hisense_ac.cpp. Only
// ESPHome logging/clock and the class shell are replaced at this test boundary.
class HisenseAC {
public:
    protocol::FrameParser parser_;
    DeviceStatus status_{};
    bool wait_for_rx{true};
    bool get_response(uint8_t input);
};
#include "response_method.inc"

static unsigned deliver(HisenseAC &ac, const Bytes &bytes) {
    unsigned accepted = 0;
    for (auto byte : bytes) {
        accepted += ac.get_response(byte);
        ++clock_ms;
    }
    return accepted;
}

int main() {
    HisenseAC ac;
    CHECK(deliver(ac, {0, 0xFF, 0xFB}) == 0);
    CHECK(ac.wait_for_rx);
    auto malformed = capture();
    malformed[20] ^= 1;
    CHECK(deliver(ac, wire(malformed)) == 0);
    CHECK(ac.wait_for_rx);
    for (size_t length : {size_t(9), size_t(21), size_t(48), size_t(81), size_t(83),
                          size_t(128), size_t(159), size_t(161), size_t(264)}) {
        CHECK(deliver(ac, wire(synthetic(length))) == 0);
        CHECK(ac.wait_for_rx);
    }
    auto unknown = synthetic();
    unknown[13] = 0x65;
    seal(unknown);
    CHECK(deliver(ac, wire(unknown)) == 0);
    CHECK(ac.wait_for_rx);
    CHECK(deliver(ac, capture()) == 1);
    CHECK(!ac.wait_for_rx && ac.status_.indoor_temperature_setting == 21);
    ac.wait_for_rx = true;
    unknown[19] = 30;
    seal(unknown);
    CHECK(deliver(ac, wire(unknown)) == 0);
    CHECK(ac.wait_for_rx && ac.status_.indoor_temperature_setting == 21);
    // A partial packet's timeout does not acknowledge anything.
    CHECK(deliver(ac, {0xF4, 0xF5, 1, 0x40, 0x49, 0xF4}) == 0);
    clock_ms += protocol::INTER_BYTE_TIMEOUT_MS;
    ac.parser_.expire(clock_ms);
    CHECK(ac.wait_for_rx);
    CHECK(deliver(ac, capture()) == 1);
    CHECK(!ac.wait_for_rx);
    ac.wait_for_rx = true;
    CHECK(deliver(ac, capture("issue_1_status_82_escaped.hex")) == 1);
    CHECK(!ac.wait_for_rx && ac.status_.indoor_temperature_setting == 19);
    ac.wait_for_rx = true;
    CHECK(deliver(ac, capture("issue_6_status_160.hex")) == 1);
    CHECK(!ac.wait_for_rx && ac.status_.indoor_temperature_setting == 16);
    return 0;
}
