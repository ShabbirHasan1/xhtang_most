#include <cstdint>
#include <climits>
#include <cassert>
#include <algorithm>

#define factor_t uint64_t

factor_t M = 0;

void init()
{
    const int n_factor = 3;
    factor_t factors[n_factor];
    const int factor_base[] = {3, 7, 11};
    const int powers[] = {35, 20, 15}; // {50, 30, 20};
    for (int i_factor = 0; i_factor < n_factor; ++i_factor)
    {
        factors[i_factor] = 1;
        for (int j = 0; j < powers[i_factor]; ++j)
        {
            factors[i_factor] *= factor_base[i_factor];
            int bits = 0;
            for (factor_t i = factors[i_factor]; i > 0; i >>= 1)
                bits++;
            assert(bits < 128 - 10);
        }
    }
    // 2 is frequent as a factor of 10
    // check other factors first
    std::swap(factors[0], factors[2]);

    M = factors[0];
}

#include "checkerV4.h"
