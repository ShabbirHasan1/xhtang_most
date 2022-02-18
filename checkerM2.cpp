#include <cstdint>

#define factor_t __uint128_t

factor_t M;
void init()
{
    const char M2[] = "104648257118348370704723099";
    M = 0;
    for (const char *c = M2; *c != '\0'; ++c)
        M = M * 10 + factor_t(*c - '0');
}

#include "checkerV4.h"
