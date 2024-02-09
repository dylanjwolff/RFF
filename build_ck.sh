#!/bin/bash
set -e

docker build -f dockerfiles/Dockerfile.base -t schedfuzz-base .
docker build -f dockerfiles/Dockerfile.period -t fuzz-period .
