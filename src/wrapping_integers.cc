#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point) {
    return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
    constexpr uint64_t modulus = uint64_t{1} << 32;
    constexpr uint64_t half = modulus / 2;
    const uint64_t offset = static_cast<uint32_t>(raw_value_ - zero_point.raw_value_);
    uint64_t result = (checkpoint & ~(modulus - 1)) | offset;
    if (result > checkpoint && result - checkpoint > half && result >= modulus) {
        result -= modulus;
    } else if (result < checkpoint && checkpoint - result > half && result <= UINT64_MAX - modulus) {
        result += modulus;
    }
    return result;
}
