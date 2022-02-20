#include "common.h"

#include <cstdlib>
#include <ctime>

const int REPEAT = 1000;
const int CNT = 200;

SmallHash<uint64_t, ssize_t> hash1[REPEAT];
SmallHashAVX<uint64_t, ssize_t> hash2[REPEAT];
int main()
{
    static pair<uint64_t, ssize_t> data[REPEAT][CNT];
    double total1 = 0, total2 = 0;
    long missed1 = 0, missed2 = 0;
    while (true)
    {
        for (int i = 0; i < REPEAT; ++i)
        {
            for (int j = 0; j < CNT; ++j)
                data[i][j] = {rand(), rand()};
        }
        {
            double t1 = get_timestamp();
            for (int i = 0; i < REPEAT; ++i)
            {
                for (int j = 0; j < CNT; ++j)
                    hash1[i][data[i][j].first] = data[i][j].second;
                for (int j = 0; j < CNT; ++j)
                    if (hash1[i].find(data[i][j].first) != data[i][j].second)
                        ++missed1;
            }
            double t2 = get_timestamp();
            total1 += t2 - t1;
            printf("no-avx %.6lf %ld\n", total1, missed1);
        }

        {
            double t1 = get_timestamp();
            for (int i = 0; i < REPEAT; ++i)
            {
                for (int j = 0; j < CNT; ++j)
                    hash2[i][data[i][j].first] = data[i][j].second;
                for (int j = 0; j < CNT; ++j)
                    if (hash2[i].find(data[i][j].first) != data[i][j].second)
                        ++missed2;
            }
            double t2 = get_timestamp();
            total2 += t2 - t1;
            printf("avx %.6lf %ld\n", total2, missed2);
        }
    }
    return 0;
}
