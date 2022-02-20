#include "common.h"

void init();

template <typename factor_t>
struct DivideAndConquer
{
    int len_dncM[M_cnt], min_len_dncM;
    factor_t inverse_of_10[M_cnt];
    factor_t pow10[N + 10][M_cnt];
    factor_t pow_inverse_of_10[N + 10][M_cnt];

    int tail = 0;
    pair<ssize_t, ssize_t> dnc_slices[N * 2];
    SmallHash<factor_t, ssize_t> start_pos_dicts[N * 2][M_cnt];

    typedef typename Sqr<factor_t>::sqr_factor_t sqr_factor_t;

    DivideAndConquer()
    {
        for (int i = 0; i < M_cnt; ++i)
            assert(M[i] % 2 != 0 && M[i] % 5 != 0);
        bool enabled = true;
        printf("divide and conquer %s\n", enabled ? "enabled" : "disabled");
        if (!enabled)
            return;

        assert(sizeof(sqr_factor_t) >= sizeof(factor_t) * 2);

        // calc the inverse of 10 (mod M)
        for (int k = 0; k < M_cnt; ++k)
        {
            factor_t a = M[k] / 10, b = M[k] % 10;

            const int x[10] = {0, 1, 0, 7, 0, 0, 0, 3, 0, 9};
            const int y[10] = {0, 0, 0, 2, 0, 0, 0, 2, 0, 8};
            assert(x[b] * b - y[b] * 10 == 1);

            inverse_of_10[k] = (M[k] - (x[b] * a + y[b]) % M[k]) % M[k];
            assert(inverse_of_10[k] * 10 % M[k] == 1);
        }

        // calc powers of 10
        for (int k = 0; k < M_cnt; ++k)
        {
            pow10[0][k] = 1;
            pow_inverse_of_10[0][k] = 1;
        }
        for (int i = 1; i <= N; ++i)
        {
            for (int k = 0; k < M_cnt; ++k)
            {
                pow10[i][k] = pow10[i - 1][k] * 10 % M[k];
                pow_inverse_of_10[i][k] = sqr_factor_t(pow_inverse_of_10[i - 1][k]) * sqr_factor_t(inverse_of_10[k]) % sqr_factor_t(M[k]);
                assert(sqr_factor_t(pow10[i][k]) * sqr_factor_t(pow_inverse_of_10[i][k]) % sqr_factor_t(M[k]) == 1);
            }
        }

        // end condition of D&C
        for (int k = 0; k < M_cnt; ++k)
        {
            len_dncM[k] = 0;
            for (factor_t i = M[k]; i > 0; i /= 10)
            {
                len_dncM[k] += 1;
            }
            // don't be too small
            assert(len_dncM[k] > (int)sizeof(factor_t));
        }
        min_len_dncM = *std::min_element(len_dncM, len_dncM + M_cnt);
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

            bool impossble[M_cnt] = {false};
            for (int k = 0; k < M_cnt; ++k)
            {
                impossble[k] = slice_len < len_dncM[k];
            }

            // find answers cross the mid
            // long answers are more possible, so we handle this case first
            ssize_t left_len = slice_len >> 1;
            ssize_t mid = left + left_len;
            ssize_t right_len = slice_len - left_len;
            factor_t part1[M_cnt] = {0};
            auto &start_pos_dict = start_pos_dicts[head];
            ssize_t max_part1_len = std::min<ssize_t>(N - 1, left_len);
            for (int part1_len = 1; part1_len <= max_part1_len; ++part1_len)
            {
                ssize_t start_pos = mid - part1_len;
                factor_t digit = buffer[start_pos] - '0';
                if (digit == 0)
                    continue;
                for (int k = 0; k < M_cnt; ++k)
                {
                    if (impossble[k])
                        continue;
                    part1[k] = (part1[k] + pow10[part1_len - 1][k] * digit) % M[k];
                    start_pos_dict[k][part1[k]] = start_pos;
                }
            }
            factor_t part2[M_cnt] = {0};
            ssize_t max_part2_len = std::min<ssize_t>(N - 1, right_len);
            for (int part2_len = 1; part2_len <= max_part2_len; ++part2_len)
            {
                ssize_t end_pos = mid + part2_len;
                factor_t digit = buffer[end_pos - 1] - '0';
                for (int k = 0; k < M_cnt; ++k)
                {
                    if (impossble[k])
                        continue;
                    part2[k] = (part2[k] * 10 + digit) % M[k];
                    if (k == 0 && digit != 0)
                        continue;

                    factor_t expected_part1 = sqr_factor_t(part2[k]) * sqr_factor_t(pow_inverse_of_10[part2_len][k]) % sqr_factor_t(M[k]);
                    expected_part1 = expected_part1 == 0 ? 0 : M[k] - expected_part1;
#ifdef DEBUG
                    assert((sqr_factor_t(expected_part1) * sqr_factor_t(pow10[part2_len][k]) + sqr_factor_t(part2[k])) % sqr_factor_t(M[k]) == 0);
#endif
                    auto it = start_pos_dict[k].find(expected_part1);
                    if (it != start_pos_dict[k].end())
                    {
                        ssize_t start_pos = it;
                        ssize_t ans_len = end_pos - start_pos;
#ifdef DEBUG
                        printf("dnc candidate: M=%d start_pos=%ld ans_len=%ld\n", k, start_pos, ans_len);
#endif
                        if (likely(ans_len <= N))
                            submitter.submit(start_pos, ans_len, DNC);
                    }
                }
            }

            // recursive handle two half, right first
            if (right_len >= min_len_dncM)
            {
                dnc_slices[tail++] = {left + left_len, right_len};
            }
            if (left_len >= min_len_dncM)
            {
                dnc_slices[tail++] = {left, left_len};
            }
        }
    }

    inline void clear()
    {
        for (int i = 0; i < tail; ++i)
        {
            for (int k = 0; k < M_cnt; ++k)
            {
                start_pos_dicts[i][k].clear();
                start_pos_dicts[i][k].reserve(N);
            }
        }
    }
};

template <typename factor_t>
struct Checker
{
    // ans_start_pos[part2_len]: moded value => ans start positions
    SmallHash<factor_t, ssize_t> ans_start_pos[N + 10][M_cnt];

    DivideAndConquer<factor_t> dnc;

    Checker()
    {
        update_ans();
    }

    void update_ans()
    {
        for (int part2_len = 1; part2_len <= N; ++part2_len)
        {
            for (int k = 0; k < M_cnt; ++k)
                ans_start_pos[part2_len][k].clear();
        }
        ssize_t max_part1_len = std::min(pos, ssize_t(N - 1));
        for (ssize_t part1_len = 1; part1_len <= max_part1_len; ++part1_len)
        {
            ssize_t start_pos = pos - part1_len;
            if (buffer[start_pos] == '0')
                continue;

            factor_t part1[M_cnt] = {0};
            for (ssize_t i = start_pos; i < pos; ++i)
            {
                factor_t digit = buffer[i] - '0';
                for (int k = 0; k < M_cnt; ++k)
                    part1[k] = (part1[k] * 10 + digit) % M[k];
            }

            ssize_t max_part2_len = N - part1_len;
            for (int part2_len = 1; part2_len <= max_part2_len; ++part2_len)
            {
                for (int k = 0; k < M_cnt; ++k)
                {
                    part1[k] = part1[k] * 10 % M[k];
                    factor_t expected_part2 = part1[k] == 0 ? 0 : M[k] - part1[k];
                    ans_start_pos[part2_len][k][expected_part2] = start_pos;
                }
            }
        }
    }

    bool on_chunk(ssize_t len)
    {
        bool submitted = false;

        factor_t part2[M_cnt] = {0};
        ssize_t max_part2_len = std::min<ssize_t>(len, N - 1);
        for (ssize_t part2_len = 1; part2_len <= max_part2_len; ++part2_len)
        {
            factor_t digit = buffer[pos + part2_len - 1] - '0';
            for (int k = 0; k < M_cnt; ++k)
            {
                part2[k] = (part2[k] * 10 + digit) % M[k];
                if (k == 0 && digit != 0)
                    continue;

                auto &possible_start_pos = ans_start_pos[part2_len][k];
                auto it = possible_start_pos.find(part2[k]);
                if (it == possible_start_pos.end())
                    continue;

                ssize_t start_pos = it;
                ssize_t ans_len = pos + part2_len - start_pos;
                submitter.submit(start_pos, ans_len, SCAN);
                submitted = true;
            }
        }

        return submitted;
    }

    void p2_calc(ssize_t len)
    {
        // calculate answers totally in the new trunk
        // we will be slow then others after previous calculation, so it's optional
        dnc.work(pos, len);
    }

    void on_chunk_done()
    {
        dnc.clear();
        update_ans();
    }
};

Checker<uint64_t> checker;
void on_chunk(ssize_t len)
{
#ifdef DEBUG
    printf("chunk len %ld\n", len);
#endif
    submitter.on_chunk();

    bool submitted = checker.on_chunk(len);
    if (!submitted)
        sched_yield();

    checker.p2_calc(len);

    sched_yield();
    pos += len;
    checker.on_chunk_done();

    submitter.on_chunk_done();
}

int main()
{
    // keep CPU powered
    int pm_qos_fd = open("/dev/cpu_dma_latency", O_RDWR);
    assert(pm_qos_fd >= 0);

    init();
    submitter.gen_submit_fd();

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    // connect
    connect_to_port(socket_fd, 10001);
    printf("connect success\n");

    // skip http header
    {
        char tmp[1024];
        while (true)
        {
            memset(tmp, 0, sizeof(tmp));
            ssize_t n = read(socket_fd, tmp, sizeof(tmp));
            rt_assert(n > 0);
            if (strstr(tmp, "\r\n"))
                break;
        }
        printf("skip http header success\n");
    }

    // main loop
    {
        // 非阻塞
        rt_assert_eq(fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL) | O_NONBLOCK), 0);

#ifdef BLOCKING
        pollfd poll_info;
        poll_info.fd = socket_fd;
        poll_info.events = POLLIN;
#endif

        while (pos + MAX_CHUNK < BUFFER_SIZE)
        {
#ifdef BLOCKING
            poll(&poll_info, 1, -1);
            if (poll_info.revents & (POLLERR | POLLHUP))
            {
                printf("Poll Error\n");
                exit(-1);
            }
#endif

            ssize_t n = read(socket_fd, buffer + pos, MAX_CHUNK);
            if (n == -1)
            {
                if (errno != EAGAIN)
                {
                    printf("Read Error\n");
                    exit(-1);
                }
                sched_yield();
                continue;
            }
            if (n == 0)
            {
                printf("EOF\n");
                exit(-1);
            }

            on_chunk(n);
            // handle page fault ahead
            memset(buffer + pos, 'f', MAX_CHUNK);
        }
    }

    printf("Buffer is full");
    return -1;
}
