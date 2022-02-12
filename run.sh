while true; do
    if [[ $1 == *.py ]]; then
        python3.8 $1
    elif [[ $1 == *.cpp ]]; then
        mkdir -p ./bin
        rm -f ./bin/$1.out
        g++ $1 -o ./bin/$1.out -O3 -std=c++17 -lpthread -Wall -Wextra -Werror -march=native -mtune=native
        ./bin/$1.out
    else
        $1
    fi
    sleep 1
done
