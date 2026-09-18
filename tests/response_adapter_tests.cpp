#include "test_support.h"
#include "hisense_ac.h"
#include "commands.h"

uint32_t esphome::test_clock = 0;
using namespace esphome;
using namespace esphome::hisense_ac;

void receive(HisenseAC &ac, uart::UARTComponent &bus, Bytes decoded, uint32_t at) {
    test_clock = at;
    auto packet = wire(decoded);
    bus.rx.insert(bus.rx.end(), packet.begin(), packet.end());
    ac.loop();
}

int main() {
    uart::UARTComponent bus;
    HisenseAC ac(&bus);
    ac.set_temperature_unit(CELSIUS);
    climate::ClimateCall call;
    call.requested_mode = climate::CLIMATE_MODE_COOL;
    call.requested_temperature = 23.4f;
    call.requested_fan = climate::CLIMATE_FAN_HIGH;
    ac.control(call);
    CHECK(bus.tx.empty());  // Actual control() only enqueues a whole call.
    ac.loop();
    CHECK(bus.tx.size() == 1 && bus.tx[0][13] == 0x66);
    auto status = capture();
    auto malformed = status;
    malformed[20] ^= 1;
    receive(ac, bus, malformed, 100);
    CHECK(bus.tx.size() == 1);
    auto unknown = synthetic(21);
    receive(ac, bus, unknown, 150);
    CHECK(bus.tx.size() == 1);
    receive(ac, bus, status, 200);  // Off -> on prerequisite.
    CHECK(bus.tx.size() == 2 && bus.tx[1] == Bytes(on, on + sizeof(on)));
    status[18] = 0x38;
    seal(status);
    receive(ac, bus, status, 300);  // A control response is not confirmation.
    CHECK(bus.tx.size() == 2);
    test_clock = 800; ac.loop();
    CHECK(bus.tx.size() == 3 && bus.tx.back()[13] == 0x66);
    receive(ac, bus, status, 900);
    CHECK(bus.tx.size() == 4 && bus.tx.back() == Bytes(mode_cool, mode_cool + CMD_SIZE));
    test_clock = 1000; ac.loop();
    test_clock = 1500; ac.loop();
    CHECK(bus.tx.back()[13] == 0x66);
    status[18] = 0x28; status[19] = 18; seal(status);
    receive(ac, bus, status, 1600);
    CHECK(bus.tx.back() == Bytes(temp_23_C, temp_23_C + CMD_SIZE));
    test_clock = 1700; ac.loop();
    test_clock = 2200; ac.loop();
    status[19] = 23; seal(status);
    receive(ac, bus, status, 2300);
    CHECK(bus.tx.back() == Bytes(speed_max, speed_max + CMD_SIZE));

    // Full queue: atomic rejection causes no requested-state publication.
    uart::UARTComponent other_bus;
    HisenseAC other(&other_bus);
    climate::ClimateCall barrier;
    barrier.requested_mode = climate::CLIMATE_MODE_OFF;
    for (unsigned i = 0; i < 8; ++i) other.control(barrier);
    const auto published = other.publications.size();
    other.control(call);
    CHECK(other.publications.size() == published);
    CHECK(other_bus.tx.empty());
    HisenseACDisplaySwitch display(&other);
    display.control(true);
    CHECK(display.publications.empty());
    climate::ClimateCall invalid;
    invalid.requested_temperature = NAN;
    invalid.requested_mode = climate::CLIMATE_MODE_HEAT;
    ac.control(invalid);
    // Each instance has independent transport and RX state.
    test_clock = 2400; other.loop();
    CHECK(other_bus.tx.size() == 1);
    CHECK(bus.tx.back() == Bytes(speed_max, speed_max + CMD_SIZE));
    return 0;
}
