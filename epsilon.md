# MOST - epsilon

## 算法

数据以 chunk 形式发送，设已收到的数字为 buf[0:L0].

采用分治方法，将答案分为两种情况：部分在新 chunk 中、完全在新 chunk 中。

### 部分在新 chunk 中

做以下平方级时间预处理

``` python
for L2 in range(1, N):
    for L1 in range(1, N - L2):
        ans_start[L2][- int(buf[start:L]) * (10 ** L2) % M].append(start)
```

新 chunk 到来时，线性时间查表即可

``` python
for L2 in range(1, min(N, len(chunk))):
    part2 = int(chunk[:L2])
    for start in ans_start[L2][part2]:
        submit(buf[start:L0] + chunk[:L2])
```

### 完全在新 chunk 中

这部分效率比较低，程序可以选择性跳过。

如果 M 与 10 不互素，则 N^2 枚举。

如果 M 与 10 互素，则分为两半，先处理答案跨过重点的情况，再递归处理两半。
具体做法与上述代码类似，但由于是实时进行，(10 ** L2) 要对 M 取逆元挪到线性查表部分。

## 系统优化

- 容器尽量静态申请内存
- 尽量开编译优化
- 预先建立提交连接池
- socket 指定 TCP_NODELAY
- "chrt -f 99 ./program" 开启实时调度

## Trick

- M1 去掉末尾的 0 后与 10 互素，可以检查 M1 / 10 与答案末尾的 0.
- M2 用 __uint128_t
- M3 分解质因数后，令 M = 某个因子（正确率 90%）
- M4 为减小计算量，令 M = 7 ^ 10（正确率 90%）
- 及时监控赛况，适当放弃得分不佳的部分
