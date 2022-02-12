import requests

N = 512
M1 = 20220209192254
M2 = 104648257118348370704723099
M3 = 125000000000000064750000000000009507500000000000294357
s = b""

USER = 'epsilon'
PASSWORD = 'suu3E5SA'

with requests.Session().get("http://47.95.111.217:10001",
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
            if num % M2 == 0 or num % M3 == 0:
                requests.post(
                    f"http://47.95.111.217:10002/submit?user={USER}&passwd={PASSWORD}",
                    data=s[i:])
                print("submit", s[i:].decode("ascii"))
