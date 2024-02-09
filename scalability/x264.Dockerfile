FROM schedfuzz-base

WORKDIR /opt
RUN apt install nasm
RUN git clone https://github.com/mirror/x264.git
WORKDIR /opt/x264
RUN ./configure
RUN make -j $(nproc)
ENV UNINST=/opt/x264/x264

COPY seeds/avi afl_testcases
ENV INPUTS=/opt/x264/afl_testcases/

ENV SUT_NAME=x264

WORKDIR /opt/sched-fuzz

RUN /usr/share/e9afl/e9afl $UNINST

ENV ARGS=" @@ --input-res 640x360 --fps 30 -o -"


ARG POS_ONLY=0

ENV TIME_BUDGET=15m

CMD AFL_NO_AFFINITY=1 AFL_FAST_CAL=1 ALWAYS_RAND=1 POWER_COE=1 ALL_PAIRS=1 POS_ONLY=${POS_ONLY} timeout $TIME_BUDGET ./fuzz.sh -i $INPUTS -o /opt/out -d -t 1000+ -- ./$SUT_NAME.afl $ARGS


