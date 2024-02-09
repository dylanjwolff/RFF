#!/bin/bash
set -e

export IMAGE=$1
echo "image key was:"
echo $IMAGE

export TIME_BUDGET=5m

VOL_DIR=$(echo $IMAGE | sed 's/\//-/g' | sed 's/:/-/g')
mkdir -p $VOL_DIR

docker run -v $(pwd)/$VOL_DIR:/opt/out -e TIME_BUDGET=$TIME_BUDGET $IMAGE
