/opt/redis/src/redis-server &
./fuzz.sh -i test -o /opt/out -d -t 8000 -- timeout 5s ./$SUT_NAME.afl
