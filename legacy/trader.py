import requests

N = 256
M1 = 20220209192254
M2 = 104648257118348370704723099  # prime
M3 = 125000000000000064750000000000009507500000000000294357
# 125000 000000 000064 750000 000000 009507 500000 000000 294357 (54 digits) = 500000 000000 000021 × 500000 000000 000107 × 500000 000000 000131
M4 = (2 ** 75) * (3 ** 50) * (7 ** 25)  # guessed by find.py
s = b""

USER = 'epsilon'
PASSWORD = 'suu3E5SA'

with requests.Session().get("http://172.1.1.119:10001",
                            stream=True,
                            headers=None) as fin:
    start_pos = 0
    for c in fin.iter_content():
        s += c
        if len(s) > N:
            start_pos += 1
        for i in range(start_pos, len(s)):
            if s[i] == ord('0'):
                continue
            num = int(s[i:])
            if num % M4 == 0:
                requests.post(
                    f"http://172.1.1.119:10002/submit?user={USER}&passwd={PASSWORD}",
                    data=s[i:])
                print("submit", s[i:].decode("ascii"))
