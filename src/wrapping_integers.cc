#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point) {
    return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
    Wrap32 wrapcheck_point = wrap(checkpoint, zero_point);
    int32_t diff = raw_value_ - wrapcheck_point.raw_value_;
    int64_t res = checkpoint + diff;
    return res >= 0 ? res : res + (1ULL << 32);
}
