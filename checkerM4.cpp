#include <cstdint>
#include <climits>
#include <cassert>
#include <algorithm>

#define factor_t uint64_t
#define FACTOR

const int n_factor = 3;
factor_t factors[n_factor];

void init()
{
    const int factor_base[] = {2, 3, 7};
    const int powers[] = {55, 35, 20}; // reduce from {75, 50, 25} to fit in uint64_t
    for (int i_factor = 0; i_factor < n_factor; ++i_factor)
    {
        factors[i_factor] = 1;
        for (int j = 0; j < powers[i_factor]; ++j)
        {
            factors[i_factor] *= factor_base[i_factor];
            assert(factors[i_factor] < UINT64_MAX / 15);
        }
    }
    // 2 is frequent as a factor of 10
    // check other factors first
    std::swap(factors[0], factors[2]);
}

#include "checkerV3.h"
