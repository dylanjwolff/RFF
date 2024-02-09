#!/bin/bash
set -e

mkdir -p $1-$TARGET_KEY
docker run -it -v $(pwd)/$1-$TARGET_KEY:/opt/out -e FUZZERS=$FUZZERS -e TARGET_KEY=$TARGET_KEY fuzz-$1
