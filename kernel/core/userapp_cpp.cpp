#include <cstdint>

extern "C" uint64_t minios_userapp_cpp_magic(void) {
    constexpr uint64_t part1 = 0x4d696E;      // "Mini"
    constexpr uint64_t part2 = 0x4f5300000000u; // "OS" suffix placeholder for readability
    return (part1 << 24u) | part2;
}
