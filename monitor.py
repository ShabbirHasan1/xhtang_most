from collections import defaultdict
from decimal import DivisionByZero
from functools import lru_cache
from datetime import date, datetime
import time
import requests
import sys

import pandas as pd

problem_def = [
    'N  = 512',
    'M1 = 20220209192254',
    'M2 = 104648257118348370704723099',
    'M3 = 125000000000000064750000000000009507500000000000294357',
    'M4 = a hidden but fixed integer, whose prime factors include and only include 2, 3 and 7',
    '',
]

M = [
    None,  # placeholder
    20220209192254,
    104648257118348370704723099,
    125000000000000064750000000000009507500000000000294357,
    (2 ** 55) * (3 ** 35) * (7 ** 20),
]

USER = 'epsilon' if len(sys.argv) == 1 else sys.argv[1]


def log(*args):
    print(*args)


class AnsStat:
    def __init__(self):
        self.cnt = 0
        self.missed = 0
        self.latency = []
        self.best_latency = []
        self.rank_cnt = defaultdict(int)
        self.rank_latency = defaultdict(list)

    def report(self, t):
        columns = [
            f'type={t}',
            '\n',
            f'cnt={self.cnt}',
            f'missed={self.missed}({0.0 if self.cnt == 0 else self.missed / self.cnt:.2%})',
            f'median(best_latency)={pd.Series(self.best_latency, dtype=int).median()}us',
            f'median(latency)={pd.Series(self.latency, dtype=int).median()}us',
            '\n',
        ]
        for i in range(1, 11):
            if self.rank_cnt[i] > 0:
                columns.append(f'rank{i}={self.rank_cnt[i]}({self.rank_cnt[i] / self.cnt:.1%})')
                columns.append(f'median={pd.Series(self.rank_latency[i]).median()}us')
            if i == 3:
                columns.append('\n')
        log('\t'.join(columns))


class AnsTracker:
    @staticmethod
    def get_ans_type(num: str):
        t = 0
        if '...' not in num:
            val = int(num)
            for i in range(1, len(M)):
                if val % M[i] == 0:
                    t = i
                    break
            assert t != 0
        return t

    @staticmethod
    def a2us(ts: str) -> int:
        return int(float(ts) * 1e6)

    def __init__(self):
        self.ranks = {}
        self.stats = defaultdict(AnsStat)
        self.fmiss = open('log/missed', 'a')

    def record_ans(self, ans: str, send_ts: str, ranks: list):
        key = (self.a2us(send_ts), ans)
        if time.time() * 1e6 - key[0] > 5e6 and key not in self.ranks:
            ans_type = self.get_ans_type(ans)
            self.ranks[key] = ranks
            for i, user in enumerate(ranks):
                if USER in user:
                    if ans_type == 0:
                        ans_type = user.split()[1]
                    self.stats[ans_type].rank_cnt[i + 1] += 1
                    latency = self.a2us(user.split()[0]) - key[0]
                    self.stats[ans_type].rank_latency[i + 1].append(latency)
                    self.stats[ans_type].latency.append(latency)
                    break
            else:
                self.stats[ans_type].missed += 1
                self.fmiss.write(f'{send_ts=}\t{ans_type=}\t{ans}\n')
                self.fmiss.flush()
            self.stats[ans_type].cnt += 1
            if len(ranks) > 0:
                self.stats[ans_type].best_latency.append(self.a2us(ranks[0].split()[0]) - key[0])

    def report(self):
        for t in sorted(self.stats, key=str):
            self.stats[t].report(t)
        if len(self.stats) > 0:
            total_cnt = sum(stat.cnt for stat in self.stats.values())
            total_miss_rate = sum(stat.missed for stat in self.stats.values()) / total_cnt
            total_rank1_rate = sum(stat.rank_cnt[1] for stat in self.stats.values()) / total_cnt
            print(f'{total_miss_rate=:.1%} {total_rank1_rate=:.1%}')


tracker = AnsTracker()
start_time = datetime.now()
while True:
    board = requests.get('http://47.95.111.217:10000/board.txt')
    if not board.ok or len(board.text) == 0:
        log('[WARN] fail to get board')
        time.sleep(60)
        continue

    lines = board.text.splitlines()
    stage = 'begin'

    solved_start = -1
    for i, line in enumerate(lines):
        if 'Leader Board' in line:
            assert i == len(problem_def)
            assert all(lines[k] == problem_def[k] for k in range(i)), f'{problem_def=} {lines[:i]=}'
            stage = 'board'
        elif 'Recent Solved' in line:
            assert stage == 'board'
            stage = 'solved'
            solved_start = i + 1
            break
    assert stage == 'solved'

    i = solved_start
    while i < len(lines):
        ans = lines[i]
        i += 1
        send_ts, sender = lines[i].split()
        assert sender == 'send'

        j = 1
        ranks = []
        while i + j < len(lines) and len(lines[i + j]) > 0:
            ranks.append(lines[i + j])
            j += 1

        tracker.record_ans(ans, send_ts, ranks)
        i += j + 1

    now = datetime.now()
    log(datetime.now(), now - start_time, time.time())
    tracker.report()
    log("")
    time.sleep(20)
