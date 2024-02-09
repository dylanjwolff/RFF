FROM schedfuzz-base

RUN apt install -y yasm nasm
WORKDIR /opt
RUN git clone https://github.com/webmproject/libvpx.git
WORKDIR /opt/libvpx
RUN git checkout f08d238

ENV PROJ_NAME=libvpx
ENV SRC_DIR=/opt/libvpx

ENV CFLAGS=-g
ENV CXXFLAGS=-g
RUN ./configure --disable-shared
RUN make

ARG SUT_NAME=vpxenc
ENV SUT_NAME=${SUT_NAME}
ENV UNINST=/opt/libvpx/$SUT_NAME

COPY seeds/avi /opt/afl-testcases
ENV INPUTS=/opt/afl-testcases

ENV ARGS=" @@ -w 640 -h 360 --threads=4 -o dest.vpx"

# RUN if [[ ${SUT_NAME} -eq "vpxdec" ]]; \ 
#       then /opt/libvpx/vpxenc $INPUTS/* -w 640 -h 360 --threads=4 -o $INPUTS/dest.vpx; \
#       ls $INPUTS | grep -v dest.vpx | xargs -I {} rm $INPUTS/{}; \
#     fi


ENV OUT=/opt/out

# Steps to run
# ------------
WORKDIR /opt/sched-fuzz

# e9afl instrument
ARG INST_FILTER=NONE
ARG DO_DYNINST=0
ARG POS_ONLY=0

# Generate dynamically filtered instrumentation
RUN if [[ ${DO_DYNINST} -eq 1 ]]; \ 
      then python3 ./dynamic-pp.py $INPUTS/*; \
      cp out.csv threading_events/$PROJ_NAME/$SUT_NAME/out.csv; \
      cp out.csv $OUT; \ 
      echo "DYNAMIC TRACE GATHERED"; \
    fi


RUN if [[ ${INST_FILTER} == "DYNAMIC" ]]; \ 
      then cp threading_events/$PROJ_NAME/$SUT_NAME/out.csv .; \
      ./selective-instrument.sh $UNINST out.csv; \
      echo "DYNAMIC FILTERING"; \
    elif [[ ${INST_FILTER} == "STATIC" ]]; \
      then cp threading_events/$PROJ_NAME/$SUT_NAME/out.csv .; \ 
      ./selective-instrument.sh $UNINST out.csv; \
      echo "STATIC FILTERING"; \
    elif [[ ${INST_FILTER} == "NONE" ]]; \
      then ./instrument.sh $UNINST; \
      echo "NO FILTERING"; \
    elif [[ ${INST_FILTER} == "ALL" ]]; \
      then echo "" > out.csv; \ 
      ./selective-instrument.sh $UNINST out.csv; \
      echo "FILTERING ALL EVENTS; PTHREAD ONLY"; \
    fi

RUN echo "filter is ${INST_FILTER}"

ENV TIME_BUDGET=15m
# run (target specific)
CMD AFL_NO_AFFINITY=1 AFL_FAST_CAL=1 ALWAYS_RAND=1 POWER_COE=1 RECORD_INCREMENTAL_EXACT_RFS=1 POS_ONLY=${POS_ONLY} timeout $TIME_BUDGET ./fuzz.sh -i $INPUTS -o $OUT -d -t 15000+ -- ./$SUT_NAME.afl $ARGS
