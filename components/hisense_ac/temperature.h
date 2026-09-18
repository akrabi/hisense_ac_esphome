#pragma once
#include <cmath>
#include <cstdint>

namespace esphome {
namespace hisense_ac {
namespace temperature {

inline float from_device(uint8_t value, bool fahrenheit) {
    return fahrenheit ? (static_cast<float>(value) - 32.0f) * (5.0f / 9.0f) : value;
}

// Validate in Celsius before casting; return the actual encodable setpoint.
inline bool normalize(float celsius, bool fahrenheit, float &normalized, uint8_t &encoded) {
    const uint8_t minimum = fahrenheit ? 61 : 16;
    const uint8_t maximum = fahrenheit ? 90 : 32;
    constexpr float EPSILON = 0.0001f;
    if (!std::isfinite(celsius) || celsius < from_device(minimum, fahrenheit) - EPSILON ||
        celsius > from_device(maximum, fahrenheit) + EPSILON)
        return false;
    const float rounded = std::round(fahrenheit ? celsius * (9.0f / 5.0f) + 32.0f : celsius);
    if (rounded < minimum || rounded > maximum) return false;
    encoded = static_cast<uint8_t>(rounded);
    normalized = from_device(encoded, fahrenheit);
    return true;
}

}  // namespace temperature
}  // namespace hisense_ac
}  // namespace esphome
