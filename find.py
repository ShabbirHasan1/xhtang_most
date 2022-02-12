import time
from collections import defaultdict

import requests

N = 512
s = b""

USER = 'epsilon'
PASSWORD = 'suu3E5SA'

with requests.Session().get("http://47.95.111.217:10001",
                            stream=True,
                            headers=None) as fin:
    start_pos = 0
    power_cnt = defaultdict(int)
    last_print = time.time()
    for c in fin.iter_content():
        s += c
        if len(s) > N:
            start_pos += 1
        for i in range(start_pos, len(s)):
            if s[i] == ord('0'):
                continue
            num = int(s[i:])
            res = num
            cnt = {2: 0, 3: 0, 7: 0}
            for mod in cnt:
                while res % mod == 0:
                    res //= mod
                    cnt[mod] += 1
            power_cnt[(cnt[2], cnt[3], cnt[7])] += 1
        now = time.time()
        if now - last_print > 5:
            last_print = now
            ROW = 10
            COLUMN = 5
            max_powers = sorted(power_cnt, key=sum, reverse=True)[:ROW*COLUMN]
            for i, powers in enumerate(max_powers):
                if i % COLUMN == 0:
                    print()
                print(f'{powers}={power_cnt[powers]}', end='\t')
            print()
