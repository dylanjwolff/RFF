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

AFL_ARGS=
OK=false
while [ $# -gt 0 ]
do
    if [ "$1" = "--" ]
    then
        shift
        OK=true
        break
    fi
    AFL_ARGS="$AFL_ARGS $1"
    shift
done

if [ $OK != true ]
then
    echo "${RED}usage${OFF}: $0 AFL_ARGS -- PROG_ARGS" >&2
    exit 1
fi

set -x
AFL_PRELOAD=./libsched.so SCHEDULE=./SCHEDULE AFL-2.57b/afl-fuzz -m none $AFL_ARGS -- $@

