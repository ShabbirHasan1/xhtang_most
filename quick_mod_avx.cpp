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

typedef uint64_t factor_t;
typedef __uint128_t sqr_factor_t;

constexpr size_t log2(sqr_factor_t num)
{
    int ans = 0;
    for (sqr_factor_t i = num; i > 0; i >>= 1)
        ans++;
    return ans;
}

inline __m256i smod(__m256i a, __m256i vd)
{
    const __m256i vd2 = _mm256_slli_epi64(vd, 1);
    const __m256i vd4 = _mm256_slli_epi64(vd, 2);
    const __m256i vd8 = _mm256_slli_epi64(vd, 3);

    auto mask = _mm256_cmpge_epu64_mask(a, vd8);
    a = _mm256_mask_sub_epi64(a, mask, a, vd8);
    mask = _mm256_cmpge_epu64_mask(a, vd4);
    a = _mm256_mask_sub_epi64(a, mask, a, vd4);
    mask = _mm256_cmpge_epu64_mask(a, vd2);
    a = _mm256_mask_sub_epi64(a, mask, a, vd2);
    mask = _mm256_cmpge_epu64_mask(a, vd);
    a = _mm256_mask_sub_epi64(a, mask, a, vd);
    return a;
}

inline double get_timestamp()
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

const int LEN = 1000000;
factor_t v[LEN];

int main()
{
    const int MULTI = 10;
    // const factor_t d = 20220217214410;
    const factor_t d = pow(11, 8);

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
            __m256i val = _mm256_set1_epi64x(0);
            __m256i vd = _mm256_set_epi64x(d, d + 1, d + 2, 1);
            for (int i = 0; i < MULTI; ++i)
            {
                for (int j = 0; j < LEN; j++)
                {
                    val = _mm256_mullo_epi64(val, _mm256_set1_epi64x(10));
                    val = _mm256_add_epi64(val, _mm256_set1_epi64x(v[j]));
                    val = smod(val, vd);
                }
            }
            factor_t val2[4] = {0};
            factor_t ds[4] = {d, d + 1, d + 2};
            for (int k = 0; k < 3; ++k)
            {
                for (int i = 0; i < MULTI; ++i)
                {
                    for (int j = 0; j < LEN; ++j)
                        val2[k] = (val2[k] * 10 + v[j]) % ds[k];
                }
            }
        }

        double t0 = get_timestamp();
        // __m256i val = _mm256_set1_epi64x(0);
        // __m256i vd = _mm256_set_epi64x(d, d + 1, d + 2, 1);
        // for (int i = 0; i < MULTI; ++i)
        // {
        //     for (int j = 0; j < LEN; j++)
        //     {
        //         val = _mm256_mullo_epi64(val, _mm256_set1_epi64x(10));
        //         val = _mm256_add_epi64(val, _mm256_set1_epi64x(v[j]));
        //         val = smod(val, vd);
        //     }
        // }

        factor_t val1[4] = {0};
        {
            factor_t ds[4] = {d, d + 1, d + 2};
            {
                for (int i = 0; i < MULTI; ++i)
                    for (int j = 0; j < LEN; ++j)
                        for (int k = 0; k < 3; ++k)
                            val1[k] = (val1[k] * 10 + v[j]) % ds[k];
            }
        }
        double t1 = get_timestamp();
        factor_t val2[4] = {0};
        {
            factor_t ds[4] = {d, d + 1, d + 2};
            for (int k = 0; k < 3; ++k)
            {
                for (int i = 0; i < MULTI; ++i)
                {
                    for (int j = 0; j < LEN; ++j)
                        val2[k] = (val2[k] * 10 + v[j]) % ds[k];
                }
            }
        }
        double t2 = get_timestamp();
        // factor_t val1[4] __attribute__((aligned(256))) = {0};
        //_mm256_store_epi64(val1, val);
        for (int k = 0; k < 4; ++k)
        {
            if (val1[k] != val2[k])
            {
                printf("k=%d %lu %lu\n", k, val1[k], val2[k]);
                assert(false);
            }
        }
        printf("smod: %.6lf mod: %.6lf\n", t1 - t0, t2 - t1);
    }
}
