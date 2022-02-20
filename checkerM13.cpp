#include <cstdint>

constexpr uint64_t pow101(uint64_t base, uint64_t power)
{
    uint64_t ans = 1;
    for (uint64_t i = 0; i < power; ++i)
        ans *= base;
    return ans;
}

constexpr uint64_t M[] = {
    // M1
    2022021721441ULL, // * 10
    // M3
    500000000000000147ULL,
    // M4 factor
    // pow101(11, 17),
};
constexpr int M_cnt = int(sizeof(M) / sizeof(M[0]));

#include "checkerV7.h"

void init()
{
}
