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
#define SERVER_IP "172.1.1.119"

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define rt_assert assert
#define rt_assert_eq(a, b) assert((a) == (b))

#ifndef factor_t
#define factor_t uint8_t
#endif

const int N = 256;
const int MAX_CHUNK = 1024;
const int MAX_STR_LEN = 1024;
const int MAX_CONTAINER_LEN = 64;

#ifdef FACTOR
extern const int n_factor;
extern factor_t factors[];
#else
extern factor_t M;
#endif

void init();

const int BUFFER_SIZE = 1 * 1024 * 1024 * 1024; // 1GB
ssize_t pos = 0;
uint8_t buffer[BUFFER_SIZE];

// 提交所使用的TCP连接池
const int SUBMIT_FD_N = 5;
int next_submit_fd = 0;
int submit_fds[SUBMIT_FD_N];

// 不同长度的http头
char headers[N + 10][MAX_STR_LEN + N];
int headers_n[N + 10];

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

// 生成提交所使用的TCP连接池
void gen_submit_fd()
{
    // {
    //     cpu_set_t mask;
    //     CPU_ZERO(&mask);
    //     CPU_SET(76, &mask);
    //     rt_assert_eq(pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask), 0);
    // }

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
}

// ans_start_pos[part2_len]: moded value => ans start positions
unordered_map<factor_t, vector<ssize_t>>
#ifdef FACTOR
    factor_start_pos[n_factor][N + 10];
#else
    ans_start_pos[N + 10];
#endif

void update_ans()
{
#ifdef FACTOR
    for (int i_factor = 0; i_factor < n_factor; ++i_factor)
    {
        auto &ans_start_pos = factor_start_pos[i_factor];
        factor_t M = factors[i_factor];
#else
    {
#endif
        for (int part2_len = 1; part2_len <= N; ++part2_len)
        {
            ans_start_pos[part2_len].clear();
        }
        ssize_t max_part1_len = std::min(pos, ssize_t(N - 1));
        for (ssize_t part1_len = 0; part1_len <= max_part1_len; ++part1_len)
        {
            ssize_t start_pos = pos - part1_len;
            if (start_pos < pos && buffer[start_pos] == '0')
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
                ans_start_pos[part2_len][expected_part2].push_back(start_pos);
            }
        }
    }
}

void on_chunk(ssize_t len)
{
    // 用户计时
    double received_time = get_timestamp();

    static double sent_times[MAX_CONTAINER_LEN];
    static int to_close_fds[MAX_CONTAINER_LEN];
    static pair<ssize_t, ssize_t> ans_slices[MAX_CONTAINER_LEN];
    int ans_cnt = 0;

#ifdef FACTOR
    factor_t part2[n_factor] = {0};
#else
    factor_t part2 = 0;
#endif
    for (ssize_t part2_len = 1; part2_len <= len; ++part2_len)
    {
        factor_t digit = buffer[pos + part2_len - 1] - '0';
#ifdef FACTOR
        for (int i_factor = 0; i_factor < n_factor; ++i_factor)
        {
            part2[i_factor] = (part2[i_factor] * 10 + digit) % factors[i_factor];
        }
        static unordered_map<ssize_t, int> votes(N);
        votes.clear();
        for (int i_factor = 0; i_factor < n_factor; ++i_factor)
        {
            auto &possible_start_pos = factor_start_pos[i_factor][part2_len];
            auto it = possible_start_pos.find(part2[i_factor]);
            if (it == possible_start_pos.end())
            {
                break;
            }
            for (ssize_t start_pos : it->second)
            {
                ++votes[start_pos];
#ifdef DEBUG
                printf("vote: i_factor=%d start_pos=%ld len=%ld\n", i_factor, start_pos, pos + part2_len - start_pos);
#endif
            }
        }
        for (auto &pair : votes)
        {
            if (pair.second < n_factor)
                continue;
            ssize_t start_pos = pair.first;
#else
        part2 = (part2 * 10 + digit) % M;

        auto &possible_start_pos = ans_start_pos[part2_len];
        auto it = possible_start_pos.find(part2);
        if (it == possible_start_pos.end())
            continue;

        for (ssize_t start_pos : it->second)
        {
#endif
            if (buffer[start_pos] == '0')
                continue;
            ssize_t ans_len = pos + part2_len - start_pos;
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
            ans_slices[ans_cnt] = std::make_pair(start_pos, ans_len);
            ++ans_cnt;
        }
    }

#ifndef ALLOW_MISS
    // calculate answers totally in the new trunk
    // we will be slow then others after previous calculation, so it's optional

    // calculate later start_pos first to avoid competing
    for (ssize_t start_pos = pos + len - 1; start_pos > pos; --start_pos)
    {
        if (buffer[start_pos] == '0')
            continue;

#ifdef FACTOR
        factor_t vals[n_factor] = {0};
#else
        factor_t val = 0;
#endif
        ssize_t max_ans_len = pos + len - start_pos;
        for (ssize_t ans_len = 1; ans_len <= max_ans_len; ++ans_len)
        {
            factor_t digit = buffer[start_pos + ans_len - 1] - '0';
#ifdef FACTOR
            factor_t all = 0;
            for (int i_factor = 0; i_factor < n_factor; ++i_factor)
            {
                vals[i_factor] = (vals[i_factor] * 10 + digit) % factors[i_factor];
                all += vals[i_factor];
            }
            if (all == 0)
#else
            val = (val * 10 + digit) % M;
            if (val == 0)
#endif
            {
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
                ans_slices[ans_cnt] = std::make_pair(start_pos, ans_len);
                ++ans_cnt;
            }
        }
    }
#endif

    if (ans_cnt > 0)
    {
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
                    printf("%.6lf ", sent_times[i] - received_time);

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
        gen_submit_fd();
    }
}

int main()
{
    assert(sizeof(factor_t) >= 8 && "please define factor_t");
    init();

    // {
    //     cpu_set_t mask;
    //     CPU_ZERO(&mask);
    //     CPU_SET(78, &mask);
    //     rt_assert_eq(pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask), 0);
    // }

    for (int i = 0; i < SUBMIT_FD_N; i++)
    {
        submit_fds[i] = -1;
    }
    gen_submit_fd();

    for (int i = 1; i <= N; i++)
    {
        headers_n[i] = sprintf(headers[i], "POST /submit?user=%s&passwd=%s HTTP/1.1\r\nContent-Length: %d\r\n\r\n", USER, PASSWORD, i);
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("socket_fd = %d\n", socket_fd);

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

    update_ans();

    // main loop
    {
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
            pos += n;
            // handle page fault ahead
            memset(buffer + pos, 'f', MAX_CHUNK);
            update_ans();
        }
    }

    printf("Buffer is full");
    return -1;
}
