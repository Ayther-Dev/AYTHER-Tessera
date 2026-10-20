#include <stdint.h>

_Static_assert(sizeof(void*) == 8, "A 64-bit target is required.");
_Static_assert(sizeof(uint32_t) == 4, "A 32-bit unsigned integer is required.");

int main(void) {
    const uint32_t value = UINT32_C(0x12345678);
    return value == UINT32_C(0x12345678) ? 0 : 1;
}
