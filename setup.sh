# ubuntu 20.04
apt update
apt install -y git tmux htop

# centos 7.9
yum install git tmux htop centos-release-scl devtoolset-10 -y
echo "source /opt/rh/devtoolset-10/enable" >> /etc/profile
source /etc/profile

# pull ocde
ssh-keygen -t rsa
git clone ssh://git@bitbucket.pinghu.tech/~xhtang/low-latency-trading-101.git
python3 -m pip install --upgrade requests

# run programs
bash run.sh a.cpp 'chrt -f 99 taskset -c 0'
