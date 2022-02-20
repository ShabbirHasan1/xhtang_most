#include "common.h"

void init();

template <typename factor_t, bool ending_zero>
struct DivideAndConquer
{
    bool enabled = false;
    int len_dncM;
    factor_t dncM;
    factor_t inverse_of_10;
    factor_t pow10[N + 10];
    factor_t pow_inverse_of_10[N + 10];

    int tail = 0;
    pair<ssize_t, ssize_t> dnc_slices[N * 2];
    SmallHash<factor_t, ssize_t> start_pos_dicts[N * 2];

    typedef typename Sqr<factor_t>::sqr_factor_t sqr_factor_t;

    DivideAndConquer(factor_t M) : dncM(M)
    {
        enabled = dncM % 2 != 0 && dncM % 5 != 0;
        enabled &= (sizeof(factor_t) <= 8);
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
        assert(len_dncM > (int)sizeof(factor_t));
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
            ssize_t max_part1_len = std::min<ssize_t>(N - 1, left_len);
            for (int part1_len = 1; part1_len <= max_part1_len; ++part1_len)
            {
                ssize_t start_pos = mid - part1_len;
                factor_t digit = buffer[start_pos] - '0';
                if (digit == 0)
                    continue;
                part1 = (part1 + pow10[part1_len - 1] * digit) % dncM;
                start_pos_dict[part1] = start_pos;
            }
            factor_t part2 = 0;
            ssize_t max_part2_len = std::min<ssize_t>(N - 1, right_len);
            for (int part2_len = 1; part2_len <= max_part2_len; ++part2_len)
            {
                ssize_t end_pos = mid + part2_len;
                factor_t digit = buffer[end_pos - 1] - '0';
                part2 = (part2 * 10 + digit) % dncM;
                if (ending_zero && digit != 0)
                    continue;

                factor_t expected_part1 = sqr_factor_t(part2) * sqr_factor_t(pow_inverse_of_10[part2_len]) % sqr_factor_t(dncM);
                expected_part1 = (dncM - expected_part1) % dncM;
#ifdef DEBUG
                assert((sqr_factor_t(expected_part1) * sqr_factor_t(pow10[part2_len]) + sqr_factor_t(part2)) % sqr_factor_t(dncM) == 0);
#endif
                auto it = start_pos_dict.find(expected_part1);
                if (it != start_pos_dict.end())
                {
                    ssize_t start_pos = it;
                    ssize_t ans_len = end_pos - start_pos;
#ifdef DEBUG
                    printf("dnc candidate: start_pos=%ld ans_len=%ld\n", start_pos, ans_len);
#endif
                    if (likely(ans_len <= N))
                        submitter.submit(start_pos, ans_len, DNC);
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
            start_pos_dicts[i].reserve(N);
        }
    }
};

struct IChecker
{
    virtual void on_chunk(ssize_t len) = 0;
    virtual void p2_calc(ssize_t len) = 0;
    virtual Stage get_stage() = 0;
    virtual void on_chunk_done() = 0;
};

template <typename factor_t, bool ending_zero = false>
struct Checker : public IChecker
{
    factor_t M;
    Stage stage;

    // ans_start_pos[part2_len]: moded value => ans start positions
    SmallHash<factor_t, ssize_t> ans_start_pos[N + 10];

    DivideAndConquer<factor_t, ending_zero> dnc;

    Checker(factor_t m) : M(m), dnc(m)
    {
        update_ans();
    }

    Stage get_stage()
    {
        return stage;
    }

    void update_ans()
    {
        for (int part2_len = 1; part2_len <= N; ++part2_len)
        {
            ans_start_pos[part2_len].clear();
        }
        ssize_t max_part1_len = std::min(pos, ssize_t(N - 1));
        for (ssize_t part1_len = 1; part1_len <= max_part1_len; ++part1_len)
        {
            ssize_t start_pos = pos - part1_len;
            if (buffer[start_pos] == '0')
                continue;

            factor_t part1 = 0;
            for (ssize_t i = start_pos; i < pos; ++i)
            {
                factor_t digit = buffer[i] - '0';
                part1 = (part1 * 10 + digit) % M;
            }

            ssize_t max_part2_len = N - part1_len;
            for (int part2_len = 1; part2_len <= max_part2_len; ++part2_len)
            {
                part1 = part1 * 10 % M;
                factor_t expected_part2 = (M - part1) % M;
                ans_start_pos[part2_len][expected_part2] = start_pos;
            }
        }
    }

    void on_chunk(ssize_t len)
    {
        bool submitted = false;

        factor_t part2 = 0;
        ssize_t max_part2_len = std::min<ssize_t>(len, N - 1);
        for (ssize_t part2_len = 1; part2_len <= max_part2_len; ++part2_len)
        {
            factor_t digit = buffer[pos + part2_len - 1] - '0';
            part2 = (part2 * 10 + digit) % M;
            if (ending_zero && digit != 0)
                continue;

            auto &possible_start_pos = ans_start_pos[part2_len];
            auto it = possible_start_pos.find(part2);
            if (it == possible_start_pos.end())
                continue;

            ssize_t start_pos = it;
            ssize_t ans_len = pos + part2_len - start_pos;
            if (ans_len <= N)
            {
                submitter.submit(start_pos, ans_len, SCAN);
                submitted = true;
            }
        }

        if (submitted)
        {
            stage = dnc.enabled ? DNC : LOOP;
        }
        else
        {
            p2_calc(len);
            stage = DONE;
        }
    }

    void p2_calc(ssize_t len)
    {
        // calculate answers totally in the new trunk
        // we will be slow then others after previous calculation, so it's optional

        if (dnc.enabled)
        {
            dnc.work(pos, len);
        }
        else
        {
#ifndef ALLOW_MISS
            // calculate later start_pos first to avoid competing
            for (ssize_t start_pos = pos + len - 1; start_pos > pos; --start_pos)
            {
                if (buffer[start_pos] == '0')
                    continue;

                factor_t val = 0;
                ssize_t max_ans_len = std::min<ssize_t>(N, pos + len - start_pos);
                for (ssize_t ans_len = 1; ans_len <= max_ans_len; ++ans_len)
                {
                    factor_t digit = buffer[start_pos + ans_len - 1] - '0';
                    val = (val * 10 + digit) % M;
                    if (ending_zero && digit != 0)
                        continue;
                    if (val == 0)
                    {
                        submitter.submit(start_pos, ans_len, LOOP);
                    }
                }
            }
        }
#endif
    }

    void on_chunk_done()
    {
        dnc.clear();
        update_ans();
    }
};

int N_CHECKER = 0;
bool shuffle = true;
IChecker *checkers[4];
void on_chunk(ssize_t len)
{
#ifdef DEBUG
    printf("chunk len %ld\n", len);
#endif
    submitter.on_chunk();

    for (int i = 0; i < N_CHECKER; ++i)
    {
        checkers[i]->on_chunk(len);
    }
    sched_yield();
    for (int i = 0; i < N_CHECKER; ++i)
    {
        if (checkers[i]->get_stage() == DNC)
        {
            checkers[i]->p2_calc(len);
        }
    }
    for (int i = 0; i < N_CHECKER; ++i)
    {
        if (checkers[i]->get_stage() == LOOP)
        {
            checkers[i]->p2_calc(len);
        }
    }

    sched_yield();
    pos += len;
    for (int i = 0; i < N_CHECKER; ++i)
    {
        checkers[i]->on_chunk_done();
    }

    if (shuffle)
        std::random_shuffle(checkers, checkers + N_CHECKER);
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
