#include <base/Common.h>

namespace m3 {
namespace net {

/**
 * Represents a MAC address
 */
class MAC {
public:
    static const size_t LEN    = 6;

    static MAC broadcast() {
        return MAC(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    explicit MAC() : _bytes() {
    }
    explicit MAC(const uint8_t *b) : MAC(b[0], b[1], b[2], b[3], b[4], b[5]) {
    }
    explicit MAC(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6) {
        _bytes[0] = b1;
        _bytes[1] = b2;
        _bytes[2] = b3;
        _bytes[3] = b4;
        _bytes[4] = b5;
        _bytes[5] = b6;
    }

    const uint8_t *bytes() const {
        return _bytes;
    }
    uint64_t value() const {
        return (uint64_t)_bytes[5] << 40 |
            (uint64_t)_bytes[4] << 32 |
            (uint64_t)_bytes[3] << 24 |
            (uint64_t)_bytes[2] << 16 |
            (uint64_t)_bytes[1] << 8 |
            (uint64_t)_bytes[0] << 0;
    }

private:
    uint8_t _bytes[LEN];
};

static inline bool operator==(const MAC &a,const MAC &b) {
    return a.bytes()[0] == b.bytes()[0] &&
        a.bytes()[1] == b.bytes()[1] &&
        a.bytes()[2] == b.bytes()[2] &&
        a.bytes()[3] == b.bytes()[3] &&
        a.bytes()[4] == b.bytes()[4] &&
        a.bytes()[5] == b.bytes()[5];
}
static inline bool operator!=(const MAC &a,const MAC &b) {
    return !operator==(a,b);
}

}
}
