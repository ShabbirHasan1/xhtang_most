#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <ctime>
#include <cmath>
#include <cassert>

#include <unistd.h>

#include <immintrin.h>

using namespace std;

typedef uint32_t factor_t;
typedef uint64_t sqr_factor_t;

constexpr size_t log2(sqr_factor_t num)
{
    int ans = 0;
    for (sqr_factor_t i = num; i > 0; i >>= 1)
        ans++;
    return ans;
}

template <factor_t d>
inline factor_t smod(factor_t a)
{
    constexpr size_t l = log2(d);
    constexpr size_t N = l + 4;
    static_assert(N + l >= sizeof(factor_t) * 8);
    static_assert(N + l < sizeof(sqr_factor_t) * 8);
    constexpr sqr_factor_t m = ((sqr_factor_t(1) << (N + l)) + (sqr_factor_t(1) << l)) / d;
    static_assert(log2(m) <= sizeof(factor_t) * 8);
    static_assert((sqr_factor_t(1) << (N + l)) <= m * d);
    static_assert(m * d <= ((sqr_factor_t(1) << (N + l)) + (sqr_factor_t(1) << l)));

    factor_t quotient = (a * m) >> (N + l);
    return a - quotient * d;
}

inline double get_timestamp()
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main()
{
    const int LEN = 1000000;
    const int MULTI = 100;
    // const factor_t d = 20220217214410;
    const factor_t d = pow(11, 8);
    vector<factor_t> v(LEN, 0);

    // for (factor_t i = 0; i < d * 10; ++i)
    // {
    //     factor_t r1 = smod<d>(i);
    //     factor_t r2 = i % d;
    //     if (r1 != r2)
    //     {
    //         printf("error: i=%lu r1=%lu r2=%lu\n", i, r1, r2);
    //         return -1;
    //     }
    // }

    while (true)
    {
        for (int i = 0; i < LEN; ++i)
            v[i] = rand() % 10;
        {
            factor_t val = 0;
            for (int i = 0; i < MULTI; ++i)
            {
                for (int j = 0; j < LEN; ++j)
                    val = smod<d>(val * 10 + v[j]);
            }
            factor_t val2 = 0;
            for (int i = 0; i < MULTI; ++i)
            {
                for (int j = 0; j < LEN; ++j)
                    val2 = (val2 * 10 + v[j]) % d;
            }
        }

        double t0 = get_timestamp();
        factor_t val = 0;
        for (int i = 0; i < MULTI; ++i)
        {
            for (int j = 0; j < LEN; ++j)
                val = smod<d>(val * 10 + v[j]);
        }
        double t1 = get_timestamp();
        factor_t val2 = 0;
        for (int i = 0; i < MULTI; ++i)
        {
            for (int j = 0; j < LEN; ++j)
                val2 = (val2 * 10 + v[j]) % d;
        }
        double t2 = get_timestamp();
        assert(val == val2);
        printf("smod: %.6lf mod: %.6lf\n", t1 - t0, t2 - t1);
    }
}
