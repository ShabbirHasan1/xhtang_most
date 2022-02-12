mkdir -p ./log
mkdir -p ./bin

while true; do
    if [[ $1 == *.py ]]; then
        python3.8 $1 | tee -a ./log/$1.log
    elif [[ $1 == *.cpp ]]; then
        rm -f ./bin/$1.out
        g++ $1 -o ./bin/$1.out -O3 -std=c++17 -lpthread -Wall -Wextra -Werror -march=native -mtune=native
        ./bin/$1.out | tee -a ./log/$1.log
    else
        $1
    fi
    sleep 1
done
