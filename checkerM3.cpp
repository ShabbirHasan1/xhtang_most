#include <cstdint>
#include <climits>
#include <cassert>
#include <algorithm>

#define factor_t uint64_t
#define FACTOR

const int n_factor = 3;
factor_t factors[n_factor] = {
    500000000000000021ULL,
    500000000000000107ULL,
    500000000000000131ULL,
};

void init()
{
}

#include "checkerV3.h"
