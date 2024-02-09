#!/bin/bash
set -e

mkdir -p $1-wip
docker run -it -v $(pwd)/$1-wip:/opt/wip fuzz-$1 /bin/bash
