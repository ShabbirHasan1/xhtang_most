import requests

N = 256
M = 20220127171443
s = b""

with requests.Session().get("http://42.62.6.5:10001", stream=True, headers=None) as fin:
    for c in fin.iter_content():
        s += c
        if len(s) > N:
            s = s[-N:]
        for i in range(len(s)):
            if int(s[i:]) % M == 0 and s[i] != ord("0"):
                requests.post("http://42.62.6.5:10002/submit?user=demo", data=s[i:])
                print("submit", s[i:].decode("ascii"))