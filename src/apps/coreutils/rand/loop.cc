#include "loop.h"

static rand_type get_rand() {
    static unsigned _last = 0x1234;
    static unsigned _randa = 1103515245;
    static unsigned _randc = 12345;
    _last = (_randa * _last) + _randc;
    return (_last / 65536) % 32768;
}

void generate(rand_type *buffer, unsigned long amount) {
    rand_type total = 0;
    for(unsigned long i = 0; i < amount; ++i)
        total += get_rand();
    buffer[0] = total;
}
