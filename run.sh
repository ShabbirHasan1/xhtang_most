mkdir -p ./log
mkdir -p ./bin

sysctl -w net.ipv4.tcp_low_latency=1
sysctl -w net.ipv4.tcp_timestamps=0
echo tsc > /sys/devices/system/clocksource/clocksource0/current_clocksource

nice_command=$2
if [[ $nice_command == "" && $USER == "root" ]]; then
    nice_command='nice -n -20'
fi

while true; do
    if [[ $1 == *.py ]]; then
        $nice_command python3.8 $1 #| tee -a ./log/$1.log
    elif [[ $1 == *.cpp ]]; then
        rm -f ./bin/$1.out
        g++ $1 -o ./bin/$1.out -O3 -std=c++17 -lpthread -Wall -Wextra -Werror -march=native -mtune=native
        $nice_command ./bin/$1.out #| tee -a ./log/$1.log
    else
        $nice_command $1
    fi
    sleep 1
done
