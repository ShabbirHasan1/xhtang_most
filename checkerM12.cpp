#include "checkerV5.h"

void init()
{
    int n_checkers = 0;
    {
        printf("init M1 checker\n");
        // 2022021721441 * 10
        checkers[n_checkers++] = new Checker<uint64_t, true>(2022021721441);
    }
    {
        printf("init M2 checker\n");
        typedef __uint128_t factor_t;
        factor_t M = 0;
        char m[] = "104648257118348370704723119";
        for (size_t i = 0; i < sizeof(m); ++i)
            M = M * 10 + m[i] - '0';
        checkers[n_checkers++] = new Checker<factor_t>(M);
    }
    N_CHECKER = n_checkers;
}
