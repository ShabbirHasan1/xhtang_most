#include <iostream>
#include <cstdint>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <cassert>
#include <fcntl.h>

using namespace std;

#define USER "epsilon"
#define PASSWORD "suu3E5SA"
#define SERVER_IP "172.1.1.119"

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define rt_assert assert
#define rt_assert_eq(a, b) assert((a) == (b))

typedef int64_t factor_t;

const int N = 256;
const int MAX_CHUNK = 1024;
const factor_t factors[] = {
    500000000000000021LL,
    500000000000000107LL,
    500000000000000131LL,
};
constexpr int n_factor = sizeof(factors) / sizeof(factors[0]);

const int BUFFER_SIZE = 1 * 1024 * 1024 * 1024; // 1GB
uint8_t *buffer = NULL;

// 提交所使用的TCP连接池
const int SUBMIT_FD_N = 8;
atomic_int submit_fds[SUBMIT_FD_N];

// 不同长度的http头
char headers[N + 10][1024];
int headers_n[N + 10];

// 获取当前时间，单位：秒
inline double get_timestamp()
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
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

    while (true)
    {
        for (int i = 0; i < SUBMIT_FD_N; i++)
            if (submit_fds[i].load() < 0)
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
                {
                    struct sockaddr_storage addr;
                    memset(&addr, 0, sizeof(addr));

                    struct sockaddr_in *addr_v4 = (struct sockaddr_in *)&addr;
                    addr_v4->sin_family = AF_INET;
                    addr_v4->sin_port = htons(10002);

                    rt_assert_eq(inet_pton(AF_INET, SERVER_IP, &addr_v4->sin_addr), 1);

                    rt_assert_eq(connect(socket_fd, (struct sockaddr *)addr_v4, sizeof(*addr_v4)), 0);
                }

                {
                    int ofd = submit_fds[i].exchange(socket_fd);
                    if (ofd >= 0)
                        close(ofd);
                }

                // printf("generate one submit fd\n");
            }

        usleep(1000);
    }
}

int main()
{
    // {
    //     cpu_set_t mask;
    //     CPU_ZERO(&mask);
    //     CPU_SET(78, &mask);
    //     rt_assert_eq(pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask), 0);
    // }

    buffer = new uint8_t[BUFFER_SIZE];

    for (int i = 0; i < SUBMIT_FD_N; i++)
    {
        submit_fds[i].store(-1);
    }

    for (int i = 1; i <= N; i++)
    {
        headers_n[i] = sprintf(headers[i], "POST /submit?user=%s&passwd=%s HTTP/1.1\r\nContent-Length: %d\r\n\r\n", USER, PASSWORD, i);
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("socket_fd = %d\n", socket_fd);

    // connect
    {
        struct sockaddr_storage addr;
        memset(&addr, 0, sizeof(addr));

        struct sockaddr_in *addr_v4 = (struct sockaddr_in *)&addr;
        addr_v4->sin_family = AF_INET;
        addr_v4->sin_port = htons(10001);

        rt_assert_eq(inet_pton(AF_INET, SERVER_IP, &addr_v4->sin_addr), 1);

        rt_assert_eq(connect(socket_fd, (struct sockaddr *)addr_v4, sizeof(*addr_v4)), 0);

        printf("connect success\n");
    }

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

    thread tt(gen_submit_fd);

    // get first data
    int64_t pos = 0;
    {
        while (pos < N * 2)
        {
            ssize_t n = read(socket_fd, buffer + pos, 1024);
            rt_assert(n > 0);
            pos += n;
        }
        printf("get first N data success\n");
    }

    factor_t num[n_factor][N + 10];
    // 使用目前收到的数据，生成最新的N个不同长度的数据
    for (int i_factor = 0; i_factor < n_factor; ++i_factor)
    {
        for (int i = 1; i <= N; i++)
        {
            num[i_factor][i] = 0;
            for (int j = pos - i; j < pos; j++)
            {
                num[i_factor][i] = (num[i_factor][i] * 10 + buffer[j] - '0') % factors[i_factor];
            }
        }
    }

    factor_t pow10[n_factor][N + 10];
    for (int i_factor = 0; i_factor < n_factor; ++i_factor)
    {
        pow10[i_factor][0] = 1;
        for (int i = 1; i <= N; i++)
        {
            pow10[i_factor][i] = (pow10[i_factor][i - 1] * 10) % factors[i_factor];
        }
    }

    // main loop
    {
        // 非阻塞
        rt_assert_eq(fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL) | O_NONBLOCK), 0);

        int next_submit_fd_pos = 0;
        while (pos + 1024 < BUFFER_SIZE)
        {
            ssize_t n = read(socket_fd, buffer + pos, 1024);

            // 用户计时
            double received_time = get_timestamp();
            double sent_times[1024];
            int sent_times_cnt = 0;

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

            int to_close_fds[1024];
            int to_close_fds_cnt = 0;

            for (int i = pos; i < pos + n; i++)
            {
                int64_t ch = buffer[i] - '0';
                for (int i_factor = 0; i_factor < n_factor; ++i_factor)
                {
                    for (int k = 1; k <= N; k++)
                    {
                        // 这里会算出负数，但是并不影响
                        num[i_factor][k] = (num[i_factor][k] * 10 + ch - (buffer[i - k] - '0') * pow10[i_factor][k]) % factors[i_factor];
                    }
                }
                int ans_k = -1;
                for (int k = 1; k <= N; k++)
                {
                    bool all_divisible = true;
                    for (int i_factor = 0; i_factor < n_factor; ++i_factor)
                    {
                        if (num[i_factor][k] != 0)
                        {
                            all_divisible = false;
                            break;
                        }
                    }
                    if (unlikely(all_divisible))
                    {
                        if (likely(buffer[i - k + 1] != '0'))
                        {
                            ans_k = k;
                            break;
                        }
                    }
                }
                if (ans_k >= 0)
                {
                    int k = ans_k;
                    memcpy(headers[k] + headers_n[k], buffer + (i - k + 1), k);
                    sent_times[sent_times_cnt++] = get_timestamp();
                    int submit_fd = submit_fds[next_submit_fd_pos].exchange(-1);
                    int ret = write(submit_fd, headers[k], headers_n[k] + k);
                    to_close_fds[to_close_fds_cnt++] = submit_fd;
                    if (ret != headers_n[k] + k)
                    {
                        printf("=====\n");
                        exit(-1);
                    }
                    next_submit_fd_pos = (next_submit_fd_pos + 1) % SUBMIT_FD_N;
                }
            }

            for (int i = 0; i < to_close_fds_cnt; i++)
            {
                close(to_close_fds[i]);
            }

            if (sent_times_cnt > 0)
            {
                for (int i = 0; i < sent_times_cnt; i++)
                {
                    printf("%.6lf ", sent_times[i] - received_time);
                }
                printf("\n");
            }

            pos += n;
            // printf("next_submit_fd_pos = %d\n", next_submit_fd_pos);
        }
    }

    tt.join();

    return 0;
}