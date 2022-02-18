#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>

#include <unordered_map>
#include <algorithm>

using std::pair;
using std::unordered_map;

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

#include <sched.h>
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
#define SERVER_IP "172.1.1.119"

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define rt_assert assert
#define rt_assert_eq(a, b) assert((a) == (b))

const int N = 512;
const int MAX_CHUNK = 1024;
const int MAX_STR_LEN = 1024;
const int MAX_CONTAINER_LEN = 8;

void init();

#ifndef sqr_factor_t
#define sqr_factor_t __uint128_t
#endif

const int BUFFER_SIZE = 1 * 1024 * 1024 * 1024; // 1GB
ssize_t pos = 0;
uint8_t buffer[BUFFER_SIZE];

// 获取当前时间，单位：秒
inline double get_timestamp()
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void connect_to_port(int socket_fd, int port)
{
    struct sockaddr_storage addr;
    memset(&addr, 0, sizeof(addr));

    struct sockaddr_in *addr_v4 = (struct sockaddr_in *)&addr;
    addr_v4->sin_family = AF_INET;
    addr_v4->sin_port = htons(port);

    rt_assert_eq(inet_pton(AF_INET, SERVER_IP, &addr_v4->sin_addr), 1);

    rt_assert_eq(connect(socket_fd, (struct sockaddr *)addr_v4, sizeof(*addr_v4)), 0);
}

enum Stage
{
    SCAN,
    DNC,
    LOOP,
    DONE,
};

struct Submitter
{
    // 不同长度的http头
    char headers[N + 10][MAX_STR_LEN + N];
    int headers_n[N + 10];

    // 提交所使用的TCP连接池
    static constexpr int SUBMIT_FD_N = 6;
    int next_submit_fd = 0;
    int submit_fds[SUBMIT_FD_N];

    double received_time;
    double sent_times[MAX_CONTAINER_LEN];
    int to_close_fds[MAX_CONTAINER_LEN];
    Stage ans_stage[MAX_CONTAINER_LEN];
    pair<ssize_t, ssize_t> ans_slices[MAX_CONTAINER_LEN];
    int ans_cnt = 0;

    Submitter()
    {
        for (int i = 0; i < SUBMIT_FD_N; i++)
        {
            submit_fds[i] = -1;
        }

        for (int i = 1; i <= N; i++)
        {
            headers_n[i] = sprintf(headers[i], "POST /submit?user=%s&passwd=%s HTTP/1.1\r\nContent-Length: %d\r\n\r\n", USER, PASSWORD, i);
        }
    }

    inline void on_chunk()
    {
        received_time = get_timestamp();
        ans_cnt = 0;
    }

    inline void submit(ssize_t start_pos, ssize_t ans_len, Stage stage)
    {
        if (ans_cnt >= SUBMIT_FD_N)
            return;
        memcpy(headers[ans_len] + headers_n[ans_len], buffer + start_pos, ans_len);

        sent_times[ans_cnt] = get_timestamp();

#ifndef DEBUG
        int submit_fd = submit_fds[next_submit_fd];
        ssize_t write_len = headers_n[ans_len] + ans_len;
        int ret = write(submit_fd, headers[ans_len], write_len);
        assert(ret == write_len && "write incomplete");
#endif

        to_close_fds[ans_cnt] = next_submit_fd;
        next_submit_fd = (next_submit_fd + 1) % SUBMIT_FD_N;

        assert(ans_len > 0);
        ans_stage[ans_cnt] = stage;
        ans_slices[ans_cnt] = {start_pos, ans_len};
        ++ans_cnt;
    }

    // 生成提交所使用的TCP连接池
    void gen_submit_fd()
    {
#ifndef DEBUG
        for (int i = 0; i < SUBMIT_FD_N; i++)
            if (submit_fds[i] < 0)
            {
                int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
                rt_assert(socket_fd >= 0);

                // no delay
                {
                    int flag = 1;
                    int result = setsockopt(socket_fd,     /* socket affected */
                                            IPPROTO_TCP,   /* set option at TCP level */
                                            TCP_NODELAY,   /* name of option */
                                            (char *)&flag, /* the cast is historical cruft */
                                            sizeof(int));  /* length of option value */
                    rt_assert_eq(result, 0);
                }

                // connect
                connect_to_port(socket_fd, 10002);
                rt_assert_eq(fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL) | O_NONBLOCK), 0);
                submit_fds[i] = socket_fd;
            }
#endif
    }

    void on_chunk_done()
    {
        if (ans_cnt > 0)
        {
#ifdef DEBUG
            for (int i = 0; i < ans_cnt; ++i)
            {
                {
                    printf("%.6lf %d ", sent_times[i] - received_time, ans_stage[i]);

                    ssize_t start_pos = ans_slices[i].first;
                    ssize_t ans_len = ans_slices[i].second;
                    fwrite(buffer + start_pos, 1, ans_len, stdout);
                    putchar('\n');
                }
            }
#else
            static pollfd pollfds[MAX_CONTAINER_LEN];
            for (int i = 0; i < ans_cnt; ++i)
            {
                pollfds[i].fd = submit_fds[to_close_fds[i]];
                pollfds[i].events = POLLIN;
                pollfds[i].revents = 0;
            }

            constexpr double max_wait = 0.005;
            double start = get_timestamp();
            int sent = 0;
            while (sent < ans_cnt)
            {
                const bool timeout = get_timestamp() - start > max_wait;
                const int poll_timeout = timeout ? 0 : 1;
                int ret = poll(pollfds, ans_cnt, poll_timeout);
                assert(ret >= 0);
                for (int i = 0; i < ans_cnt; ++i)
                {
                    if (pollfds[i].fd < 0)
                        continue;
                    int fd = pollfds[i].fd;

                    bool received, bad;
                    ssize_t n_read = 0;
                    static char response[MAX_STR_LEN];
                    if (pollfds[i].revents & (POLLERR | POLLHUP))
                    {
                        received = false;
                        bad = true;
                    }
                    else if (pollfds[i].revents & POLLIN)
                    {
                        n_read = read(fd, response, sizeof(response));
                        received = n_read > 0;
                        bad = n_read == 0 || (n_read < 0 && errno != EAGAIN);
                    }
                    else
                    {
                        received = false;
                        bad = false;
                    }

                    if (!received && !bad)
                    {
                        int remained_bytes;
                        ioctl(fd, TIOCOUTQ, &remained_bytes);
                        received = remained_bytes == 0;
                    }

                    if (received || timeout || bad)
                    {
                        printf("%.6lf %d ", sent_times[i] - received_time, ans_stage[i]);

                        ssize_t start_pos = ans_slices[i].first;
                        ssize_t ans_len = ans_slices[i].second;
                        fwrite(buffer + start_pos, 1, ans_len, stdout);

                        puts(received ? "" : "(timeout)");
                        if (n_read > 0)
                        {
                            response[std::min<ssize_t>(n_read, sizeof(response) - 1)] = '\0';
                            puts(response);
                        }

                        if (!received)
                        {
                            submit_fds[to_close_fds[i]] = -1;
                            close(fd);
                        }
                        sent += 1;
                        pollfds[i].fd = -1;
                    }
                }
            }
#endif

            gen_submit_fd();
        }
    }
} submitter;

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
    unordered_map<factor_t, ssize_t> start_pos_dicts[N * 2];

    DivideAndConquer(factor_t M) : dncM(M)
    {
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
                    ssize_t start_pos = it->second;
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
    unordered_map<factor_t, ssize_t> ans_start_pos[N + 10];

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

            ssize_t start_pos = it->second;
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

    std::random_shuffle(checkers, checkers + N_CHECKER);
    submitter.on_chunk_done();
}

int main()
{
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

        pollfd poll_info;
        poll_info.fd = socket_fd;
        poll_info.events = POLLIN;

        while (pos + MAX_CHUNK < BUFFER_SIZE)
        {
            poll(&poll_info, 1, -1);
            if (poll_info.revents & (POLLERR | POLLHUP))
            {
                printf("Poll Error\n");
                exit(-1);
            }

            ssize_t n = read(socket_fd, buffer + pos, MAX_CHUNK);
            if (n == -1)
            {
                if (errno != EAGAIN)
                {
                    printf("Read Error\n");
                    exit(-1);
                }
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
