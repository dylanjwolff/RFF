FROM schedfuzz-base

# build / install target
WORKDIR /opt/sched-fuzz
ENV SUT_NAME=UnrollInlineSafeStack
ENV UNINST=/opt/sched-fuzz/test/$SUT_NAME
RUN cd test; clang++ -lpthread -g $UNINST.cpp -o $UNINST
ENV INPUTS=/opt/sched-fuzz/input

RUN mkdir -p $INPUTS
RUN echo "hi" >  $INPUTS/test.txt
# e9afl instrument
WORKDIR /opt/sched-fuzz
RUN ./instrument.sh $UNINST

# run (target specific)
CMD INPUT=10000 ./fuzz.sh -i $INPUTS -o /opt/out -d -- ./$SUT_NAME.afl @@


