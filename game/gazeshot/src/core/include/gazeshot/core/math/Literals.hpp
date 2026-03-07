#pragma once

#include <cmath>

namespace gazeshot::core::math::literals {

// 45.0_deg -> 0.78539816... radians
constexpr float operator""_deg(long double degrees) {
  return static_cast<float>(degrees * M_PI / 180.0L);
}

constexpr float operator""_deg(unsigned long long degrees) {
  return static_cast<float>(static_cast<long double>(degrees) * M_PI / 180.0L);
}

}  // namespace gazeshot::core::math::literals