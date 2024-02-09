
FROM schedfuzz-base

RUN apt install -y tclsh libz-dev

WORKDIR /opt
RUN git clone https://github.com/sqlite/sqlite.git
WORKDIR /opt/sqlite
RUN git checkout 739edb9

ENV PROJ_NAME=sqlite3
ENV SUT_NAME=threadtest5
ENV SRC_DIR=/opt/sqlite

ENV CFLAGS="-g -DSQLITE_THREADSAFE=1"
ENV CXXFLAGS="-g -DSQLITE_THREADSAFE=1"
RUN ./configure --disable-shared

RUN make -j $(nproc) $SUT_NAME 
ENV UNINST=$SRC_DIR/$SUT_NAME

RUN wget https://raw.githubusercontent.com/google/fuzzbench/master/benchmarks/sqlite3_ossfuzz/ossfuzz.dict
ENV DICT=/opt/sqlite/ossfuzz.dict

# threadtest5 is fixed-input, so no real input files
RUN mkdir -p /opt/sqlite/tt5inputs
ENV INPUTS=/opt/sqlite/tt5inputs
RUN echo "hi" > $INPUTS/hi.txt 

# Remove large inputs
# RUN cd $INPUTS; find ./ -size +10k -type f | xargs rm

# only take 1st arg, so i.e. a random input
ENV ARGS=" "

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
CMD AFL_NO_AFFINITY=1 AFL_FAST_CAL=1 ALWAYS_RAND=1 POWER_COE=1 ALL_PAIRS=1 POS_ONLY=${POS_ONLY} timeout $TIME_BUDGET ./fuzz.sh -i $INPUTS -o $OUT -d -t 15000+ -x $DICT -- ./$SUT_NAME.afl $ARGS


