#!/bin/bash

usage()
{
    echo "$0 NCORES"
}
test_1()
{
    while true
    do
        true
    done
}

test_2()
{
    local m=1
    while true
    do
        local n=1
        while true
        do
            n=`expr $n + 1`
            if [[ $n -gt 5000 ]]; then
                break;
            fi
        done
        echo "TextCPU: $m"
        m=`expr $m + 1`
    done
}

if [[ -z "$1" ]]; then
    usage
    exit 1
fi

ncores=$1

for ((i=0; i < ncores; i++))
do
    test_1 &
done

while true
do
    sleep 1
done
