#!/bin/sh

if [ -t 1 ]
then
    RED="\033[31m"
    GREEN="\033[32m"
    YELLOW="\033[33m"
    BOLD="\033[1m"
    OFF="\033[0m"
else
    RED=
    GREEN=
    YELLOW=
    BOLD=
    OFF=
fi

SCHEDULES=
while [ $# -gt 0 ]
do
    if [ "$1" = "--" ]
    then
        shift
        OK=true
        break
    fi
    SCHEDULES="$SCHEDULES $1"
    shift
done

if [ $OK != true ]
then
    echo "${RED}usage${OFF}: $0 [SCHEDULE ...] -- prog.afl [ARGS ...]"
    exit 1
fi

gcc -O2 -o crc32 crc32.c -msse4.2

for FILE in $SCHEDULES
do
    echo "SCHEDULE=$FILE LD_PRELOAD=./libsched.so $@"
    SCHEDULE=$FILE LD_PRELOAD=./libsched.so $@ | tee out.tmp
    echo -n "${GREEN}"
    ./crc32 out.tmp
    echo -n "${OFF}"
done

