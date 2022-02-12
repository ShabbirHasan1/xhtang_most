while true; do
    if [[ $1 == *.py ]]; then
        python3.8 $1
    elif [[ $1 == *.cpp ]]; then
        g++ $1 -o $1.out -O3 -std=c++17 -lpthread -lm -Wall
        ./$1.out
    else
        $1
    fi
    sleep 1
done
