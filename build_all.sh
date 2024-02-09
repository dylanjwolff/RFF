#!/bin/bash
set -e

docker build -f dockerfiles/Dockerfile.qlearning_base -t schedfuzz-base .
docker build -f dockerfiles/Dockerfile.period -t fuzz-period-qlearning .

docker build -f dockerfiles/Dockerfile.pct_base -t schedfuzz-base .
docker build -f dockerfiles/Dockerfile.period -t fuzz-period-pct .

docker build -f dockerfiles/Dockerfile.base -t schedfuzz-base .
docker build -f dockerfiles/Dockerfile.period -t fuzz-period .
