#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>

#include <vector>
#include <unordered_map>
#include <algorithm>

using std::pair;
using std::unordered_map;
using std::vector;

namespace std
{
    template <>
    struct hash<__uint128_t>
    {
        inline size_t operator()(__uint128_t v) const
        {
            return (v >> 64) ^ (v & 0xFFFFFFFFFFFFFFFFull);
        }
    };
}

#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>

#define USER "epsilon"
#define PASSWORD "suu3E5SA"
#define SERVER_IP "47.95.111.217"

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define rt_assert assert
#define rt_assert_eq(a, b) assert((a) == (b))

#define factor_t uint64_t
#define FACTOR

const int n_factor = 3;
factor_t factors[n_factor] = {
    500000000000000021ULL,
    500000000000000107ULL,
    500000000000000131ULL,
};

#ifndef factor_t
#define factor_t uint8_t
#endif

const int N = 512;
const int MAX_CHUNK = 1024;
const int MAX_STR_LEN = 1024;
const int MAX_CONTAINER_LEN = 8;

#ifdef FACTOR
extern const int n_factor;
extern factor_t factors[];
#else
extern factor_t M;
#endif

void init()
{
}

#ifndef sqr_factor_t
#define sqr_factor_t __uint128_t
#endif

const int BUFFER_SIZE = 1 * 1024 * 1024 * 1024; // 1GB
char buffer[] = "24879346286008077632215728039136466586565477481650864883505495431676995638256905961010244453405317691200494341468512677504019685589781894722320586694324299172935110669923866102626526564635234731";

struct Submitter
{
    void submit(ssize_t start_pos, ssize_t ans_len)
    {
        printf("start_pos=%ld ans_len=%ld\n", start_pos, ans_len);
    }
} submitter;

struct DivideAndConquer
{
    template <typename T>
    class SmallVector
    {
    private:
        int len;

    public:
        T elements[MAX_CONTAINER_LEN];

        SmallVector() : len(0) {}

        inline void push_back(T val)
        {
            if (likely(len < MAX_CONTAINER_LEN))
                elements[len++] = val;
        }

        inline int size()
        {
            return len;
        }

        inline T operator[](int index)
        {
            return elements[index];
        }
    };

    bool enabled = false;
    factor_t dncM = 0;
    factor_t inverse_of_10 = 0;
    int len_dncM = 0;
    factor_t pow10[N + 10];
    factor_t pow_inverse_of_10[N + 10];

    int tail = 0;
    pair<ssize_t, ssize_t> dnc_slices[N * 20];
    unordered_map<factor_t, SmallVector<ssize_t>> start_pos_dicts[N * 20];

    void init()
    {
#ifdef FACTOR
        dncM = factors[0];
#else
        dncM = M;
#endif
        enabled = dncM % 2 != 0 && dncM % 5 != 0;
        enabled &= (sizeof(factor_t) == 8);
        printf("divide and conquer %s\n", enabled ? "enabled" : "disabled");
        if (!enabled)
            return;

        assert(sizeof(sqr_factor_t) >= sizeof(factor_t) * 2);

        // calc the inverse of 10 (mod dncM)
        factor_t a = dncM / 10, b = dncM % 10;

        const int x[10] = {0, 1, 0, 7, 0, 0, 0, 3, 0, 9};
        const int y[10] = {0, 0, 0, 2, 0, 0, 0, 2, 0, 8};
        assert(x[b] * b - y[b] * 10 == 1);

        inverse_of_10 = (dncM - (x[b] * a + y[b]) % dncM) % dncM;
        assert(inverse_of_10 * 10 % dncM == 1);

        // calc powers of 10
        pow10[0] = 1;
        pow_inverse_of_10[0] = 1;
        for (int i = 1; i <= N; ++i)
        {
            pow10[i] = pow10[i - 1] * 10 % dncM;
            pow_inverse_of_10[i] = sqr_factor_t(pow_inverse_of_10[i - 1]) * sqr_factor_t(inverse_of_10) % sqr_factor_t(dncM);
            assert(sqr_factor_t(pow10[i]) * sqr_factor_t(pow_inverse_of_10[i]) % sqr_factor_t(dncM) == 1);
        }

        // end condition of D&C
        len_dncM = 0;
        for (factor_t i = dncM; i > 0; i /= 10)
        {
            len_dncM += 1;
        }
        assert(len_dncM > 10);
    }

    void work(ssize_t range_left, ssize_t range_len)
    {
        int head = 0;
        tail = 1;
        dnc_slices[0] = {range_left, range_len};
        while (head < tail)
        {
            ssize_t left = dnc_slices[head].first;
            ssize_t slice_len = dnc_slices[head].second;
            ++head;

            // find answers cross the mid
            // long answers are more possible, so we handle this case first
            ssize_t left_len = slice_len >> 1;
            ssize_t mid = left + left_len;
            ssize_t right_len = slice_len - left_len;
            factor_t part1 = 0;
            auto &start_pos_dict = start_pos_dicts[head];
            for (int part1_len = 1; part1_len <= left_len; ++part1_len)
            {
                ssize_t start_pos = mid - part1_len;
                factor_t digit = buffer[start_pos] - '0';
                if (digit == 0)
                    continue;
                part1 = (part1 + pow10[part1_len - 1] * digit) % dncM;
                start_pos_dict[part1].push_back(start_pos);
            }
            factor_t part2 = 0;
            for (int part2_len = 1; part2_len <= right_len; ++part2_len)
            {
                ssize_t end_pos = mid + part2_len;
                factor_t digit = buffer[end_pos - 1] - '0';
                part2 = (part2 * 10 + digit) % dncM;

                factor_t expected_part1 = sqr_factor_t(part2) * sqr_factor_t(pow_inverse_of_10[part2_len]) % sqr_factor_t(dncM);
                expected_part1 = (dncM - expected_part1) % dncM;
#ifdef DEBUG
                assert((sqr_factor_t(expected_part1) * sqr_factor_t(pow10[part2_len]) + sqr_factor_t(part2)) % sqr_factor_t(dncM) == 0);
#endif

                auto it = start_pos_dict.find(expected_part1);
                if (it != start_pos_dict.end())
                {
                    for (int i = 0; i < it->second.size(); ++i)
                    {
                        ssize_t start_pos = it->second[i];
                        ssize_t ans_len = end_pos - start_pos;
#ifdef DEBUG
                        printf("dnc candidate: start_pos=%ld ans_len=%ld\n", start_pos, ans_len);
#endif
#ifdef FACTOR
                        bool is_ans = true;
                        for (int i_factor = 1; i_factor < n_factor; ++i_factor)
                        {
                            factor_t val = 0;
                            for (ssize_t j = 0; j < ans_len; ++j)
                            {
                                val = (val * 10 + buffer[start_pos + j] - '0') % factors[i_factor];
                            }
                            if (val != 0)
                            {
                                is_ans = false;
                                break;
                            }
                        }
                        if (!is_ans)
                        {
                            continue;
                        }
#endif
                        submitter.submit(start_pos, ans_len);
                    }
                }
            }

            // recursive handle two half, right first
            if (right_len >= len_dncM)
            {
                dnc_slices[tail++] = {left + left_len, right_len};
            }
            if (left_len >= len_dncM)
            {
                dnc_slices[tail++] = {left, left_len};
            }
        }
    }

    inline void clear()
    {
        for (int i = 0; i < tail; ++i)
        {
            start_pos_dicts[i].clear();
        }
    }
} dnc;

int main()
{
    init();
    dnc.init();
    dnc.work(0, strlen(buffer));
    return 0;
}
