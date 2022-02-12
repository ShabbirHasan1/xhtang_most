if [[ $1 == *.cpp ]]; then
    mkdir -p ./bin
    rm -f ./bin/$1.dbg
    g++ $1 -o ./bin/$1.dbg -g -std=c++17 -lpthread -Wall -Wextra -Werror -DDEBUG && gdb ./bin/$1.dbg
else
    gdb $1
fi
