#include <cstdint>
#include <climits>
#include <cassert>

#define factor_t uint64_t
#define FACTOR

const int n_factor = 3;
factor_t factors[n_factor];

void init()
{
    const int factor_base[] = {2, 3, 7};
    const int powers[] = {55, 35, 20}; // reduce from {75, 50, 25} to save computation cost
    for (int i_factor = 0; i_factor < n_factor; ++i_factor)
    {
        factors[i_factor] = 1;
        for (int j = 0; j < powers[i_factor]; ++j)
            factors[i_factor] *= factor_base[i_factor];
        assert(factors[i_factor] < UINT64_MAX / 15);
    }
}

#include "checkerV2.h"
