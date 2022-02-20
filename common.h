#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>

#include <algorithm>

using std::pair;

#include <strings.h>
#include <sched.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>

#include <immintrin.h>

#define USER "epsilon"
#define PASSWORD "suu3E5SA"
#define SERVER_IP "172.1.1.119"

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define rt_assert assert
#define rt_assert_eq(a, b) assert((a) == (b))

const int N = 256;
const int MAX_CHUNK = 1024;
const int MAX_STR_LEN = 1024;
const int MAX_CONTAINER_LEN = 4;

const int BUFFER_SIZE = 1 * 1024 * 1024 * 1024; // 1GB
ssize_t pos = 0;
uint8_t buffer[BUFFER_SIZE];

// 获取当前时间，单位：秒
inline double get_timestamp()
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME_COARSE, &ts);
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

template <typename K, typename V>
struct SmallHash
{
    static constexpr size_t CAPACITY = N * N / 2;
    pair<K, V> table[CAPACITY];

    SmallHash()
    {
        clear();
    }

    inline V &operator[](K k)
    {
        table[k % CAPACITY].first = k;
        return table[k % CAPACITY].second;
    }

    inline void clear()
    {
        std::fill(table, table + CAPACITY, pair<K, V>(0, -1));
    }

    inline void reserve(int)
    {
        ;
    }

    inline V find(K k)
    {
        if (table[k % CAPACITY].first == k)
            return table[k % CAPACITY].second;
        return -1;
    }

    inline V end()
    {
        return -1;
    }
};

template <typename K, typename V>
struct SmallHashAVX
{
    static constexpr size_t CAPACITY = N * MAX_CONTAINER_LEN;
    static constexpr V NONE = -1;
    pair<K, V> table[CAPACITY][MAX_CONTAINER_LEN] __attribute__((aligned(256)));

    SmallHashAVX()
    {
        static_assert(sizeof(K) == 8);
        static_assert(sizeof(V) == 8);
        static_assert(sizeof(pair<K, V>) == sizeof(K) + sizeof(V));
        static_assert(sizeof(table[0]) * 8 == 512);
        static_assert(MAX_CONTAINER_LEN == 4);
        clear();
    }

    inline V &operator[](K k)
    {
        auto &entry = table[k % CAPACITY];
        __m256i kvec = _mm256_i64gather_epi64(reinterpret_cast<long long *>(entry), _mm256_set_epi64x(0, 2, 4, 6), 8);
        __m256i vvec = _mm256_i64gather_epi64(reinterpret_cast<long long *>(entry), _mm256_set_epi64x(1, 3, 5, 7), 8);
        __m256i found_k = _mm256_cmpeq_epi64(kvec, _mm256_set1_epi64x(k));
        __m256i empty_v = _mm256_cmpeq_epi64(vvec, _mm256_set1_epi64x(NONE));
        __m256i found = _mm256_or_si256(found_k, empty_v);
        int mask = _mm256_movemask_epi8(found);
        int index = ffs(mask);
        if (index > 0)
            index = (index - 1) / 8;
#ifdef DEBUG
        assert(index >= 0 && index < MAX_CONTAINER_LEN);
        assert(entry[index].first == k || entry[index].second = NONE);
#endif
        entry[index].first = k;
        return entry[index].second;
    }

    inline void clear()
    {
        std::fill(table[0], table[0] + CAPACITY * MAX_CONTAINER_LEN, pair<K, V>(0, NONE));
    }

    inline void reserve(int)
    {
        ;
    }

    inline V find(K k)
    {
        auto &entry = table[k % CAPACITY];
        __m256i kvec = _mm256_i64gather_epi64(reinterpret_cast<long long *>(entry), _mm256_set_epi64x(0, 2, 4, 6), 8);
        __m256i found_k = _mm256_cmpeq_epi64(kvec, _mm256_set1_epi64x(k));
        int mask = _mm256_movemask_epi8(found_k);
        int index = ffs(mask);
#ifdef DEBUG
        assert(index >= 0 && index <= MAX_CONTAINER_LEN * 8);
        assert(index == 0 || entry[(index - 1) / 8].first == k);
#endif
        V res = index != 0 ? entry[(index - 1) / 8].second : NONE;
        return res;
    }

    inline V end()
    {
        return NONE;
    }
};

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
    static constexpr int SUBMIT_FD_N = MAX_CONTAINER_LEN;
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
        if (unlikely(ans_cnt >= SUBMIT_FD_N))
            return;

#ifndef DEBUG
        struct iovec data[2] =
            {
                {headers[ans_len], (size_t)headers_n[ans_len]},
                {buffer + start_pos, (size_t)ans_len},
            };
        int submit_fd = submit_fds[next_submit_fd];
        ssize_t write_len = headers_n[ans_len] + ans_len;
        int ret = writev(submit_fd, data, 2);
        assert(ret == write_len && "write incomplete");
#endif

        sent_times[ans_cnt] = get_timestamp();
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

template <typename factor_t>
struct Sqr
{
    typedef uint8_t sqr_factor_t;
};

template <>
struct Sqr<uint64_t>
{
    typedef __uint128_t sqr_factor_t;
};

template <>
struct Sqr<uint32_t>
{
    typedef uint64_t sqr_factor_t;
};
