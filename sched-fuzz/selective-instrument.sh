#!/bin/sh
set -e


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

if [ ! -x /usr/share/e9afl/e9afl ]
then
    echo "${RED}error${OFF}: please install ${YELLOW}e9afl_0.6.0_amd64.deb${OFF} first" >&2
    exit 1
fi

if [ $# = 0 ]
then
    echo "${RED}usage${OFF}: $0 binary [e9tool-options]" >&2
    exit 1
fi

BINARY=`which "$1"`
BASENAME=`basename "$BINARY"`
shift
CSV=$(echo $1 | sed 's/.csv//g')
shift

set +e
PIE=`file "$BINARY" | grep "shared object"`
set -e
EXTRA=
if [ "$PIE" = "" ]
then
    EXTRA="--option --mem-lb=0x300000"
fi

(cd AFL-2.57b; make)
clang++ -std=c++17 -fPIC -shared -I include -o libsched.so libsched.cpp -ldl -pthread -msse4.2 -g
e9compile sched.c

set -x

"/usr/share/e9afl/e9tool" \
	-o "${BASENAME}.afl" \
	-E '".plt"' -E '".plt.got"' -O2 --option --mem-granularity=4096 \
	-M 'plugin("/usr/share/e9afl/e9AFLPlugin.so").match()' \
	-P 'plugin("/usr/share/e9afl/e9AFLPlugin.so").patch()' \
	--plugin="/usr/share/e9afl/e9AFLPlugin.so":--counter=classic \
	--plugin="/usr/share/e9afl/e9AFLPlugin.so":-Oblock=default \
	--plugin="/usr/share/e9afl/e9AFLPlugin.so":-Oselect=default \
	--plugin="/usr/share/e9afl/e9AFLPlugin.so":--path='/usr/share/e9afl' \
    -M "mem[0].access == rw && mem[0].seg == nil && addr == ${CSV}[0]" \
    -P 'mem_wri(asm, (static)addr, &mem[0], mem[0].size, F)@sched' \
    -M "mem[1].access == rw && mem[1].seg == nil && addr == ${CSV}[0]" \
    -P 'mem_wri(asm, (static)addr, &mem[1], mem[1].size, F)@sched' \
    -M "mem[0].access == r && mem[0].seg == nil && addr == ${CSV}[0]" \
    -P 'mem_ri(asm, (static)addr, &mem[0], mem[0].size, F)@sched' \
    -M "mem[1].access == r && mem[1].seg == nil && addr == ${CSV}[0]" \
    -P 'mem_ri(asm, (static)addr, &mem[1], mem[1].size, F)@sched' \
    -M "mem[0].access == w && mem[0].seg == nil && addr == ${CSV}[0]" \
    -P 'mem_wi(asm, (static)addr, &mem[0], mem[0].size, F)@sched' \
    -M "mem[1].access == w && mem[1].seg == nil && addr == ${CSV}[0]" \
    -P 'mem_wi(asm, (static)addr, &mem[1], mem[1].size, F)@sched' \
	--option --log=false $EXTRA $@ -- "$BINARY"

