#include "checkerV6a.h"

void init()
{
    int n_checkers = 0;
    {
        printf("init M3 checker\n");
        checkers[n_checkers++] = new Checker<uint64_t>(500000000000000147ULL);
    }
    {
        printf("init M4 checker\n");
        const int n_factor = 3;
        const int factor_base[] = {3, 7, 11};
        const int powers[] = {16, 10, 8}; // {50, 30, 20};
        typedef uint32_t factor_t;
        factor_t factors[n_factor];
        for (int i_factor = 0; i_factor < n_factor; ++i_factor)
        {
            factors[i_factor] = 1;
            for (int j = 0; j < powers[i_factor]; ++j)
            {
                factors[i_factor] *= factor_base[i_factor];
                assert(factors[i_factor] < UINT32_MAX / 15);
            }
        }
        checkers[n_checkers++] = new Checker<factor_t>(factors[2]);
    }
    N_CHECKER = n_checkers;
}