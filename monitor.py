from collections import defaultdict
from functools import lru_cache
from datetime import datetime
import time
import requests

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

USER = 'epsilon'


def log(*args):
    print(*args)


class AnsStat:
    def __init__(self, t):
        self.t = t
        self.cnt = 0
        self.missed = 0
        self.rank_cnt = defaultdict(int)

    def report(self):
        columns = [
            f'type={self.t}',
            f'cnt={self.cnt}',
            f'missed={self.missed}({0.0 if self.cnt == 0 else self.missed / self.cnt:.2%})'
        ]
        for i in range(1, 11):
            if self.rank_cnt[i] > 0:
                columns.append(f'rank{i}={self.rank_cnt[i]}({0.0 if self.cnt == 0 else self.rank_cnt[i] / self.cnt:.2%})')
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

    def __init__(self):
        self.ranks = {}
        self.stats = [AnsStat(t) for t in range(len(M))]
        self.fmiss = open('log/missed', 'a')

    def record_ans(self, ans: str, send_ts: str, ranks: list):
        key = (int(float(send_ts) * 1e6), ans)
        if time.time() * 1e6 - key[0] > 5e6 and key not in self.ranks:
            ans_type = self.get_ans_type(ans)
            self.ranks[key] = ranks
            self.stats[ans_type].cnt += 1
            for i, user in enumerate(ranks):
                if USER in user:
                    self.stats[ans_type].rank_cnt[i + 1] += 1
                    break
            else:
                self.stats[ans_type].missed += 1
                self.fmiss.write(f'{send_ts=}\t{ans_type=}\t{ans}\n')
                self.fmiss.flush()

    def report(self):
        for t in range(len(M)):
            self.stats[t].report()


tracker = AnsTracker()

while True:
    board = requests.get('http://47.95.111.217:10000/board.txt')
    if not board.ok or len(board.text) == 0:
        log('[WARN] fail to get board')
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

    log(datetime.now(), time.time())
    tracker.report()
    log("")
    time.sleep(10)
