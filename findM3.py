import time
from collections import defaultdict

import requests

N = 256
s = b""

USER = 'epsilon'
PASSWORD = 'suu3E5SA'

M3 = 125000000000000140750000000000052207500000000006359661
factors = [
    500000000000000147,
    500000000000000207,
    500000000000000209,
]


def is_submit(num):
    return num % factors[0] == 0


def is_ans(num):
    return num % M3 == 0


with requests.Session().get("http://172.1.1.119:10001",
                            stream=True,
                            headers=None) as fin:
    start_pos = 0
    last_print = time.time()
    submit_cnt = 0
    ans_cnt = 0
    for c in fin.iter_content():
        s += c
        if len(s) > N:
            start_pos += 1
        for i in range(start_pos, len(s)):
            if s[i] == ord('0'):
                continue
            num = int(s[i:])
            if is_submit(num):
                submit_cnt += 1
                if is_ans(num):
                    ans_cnt += 1
        now = time.time()
        if now - last_print > 5:
            last_print = now
            if submit_cnt > 0:
                print(f'{submit_cnt=} {ans_cnt=} {ans_cnt / submit_cnt=}')
