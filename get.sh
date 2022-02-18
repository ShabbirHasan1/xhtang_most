target=$1

if [[ $1 == "" ]]; then
    echo "Usage: get.sh <target>"
    exit 1
fi

server='172.1.1.119'
if [[ $1 == "board"]]; then
    curl $server:10000/board.txt
elif [[ $1 == "most" ]]; then
    curl $server:10000/most.txt
elif [[ $1 == "png" ]]; then
    curl $server:10000/board.png
else
    echo unknown target: $1
fi
