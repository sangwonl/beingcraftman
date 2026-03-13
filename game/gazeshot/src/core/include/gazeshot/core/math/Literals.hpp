#pragma once

namespace gazeshot::core::math::literals {

inline constexpr long double PI_LD = 3.14159265358979323846L;

// 45.0_deg -> 0.78539816... radians
constexpr float operator""_deg(long double degrees) {
  return static_cast<float>(degrees * PI_LD / 180.0L);
}

constexpr float operator""_deg(unsigned long long degrees) {
  return static_cast<float>(static_cast<long double>(degrees) * PI_LD / 180.0L);
}

}  // namespace gazeshot::core::math::literals