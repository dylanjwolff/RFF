import sys, os

def preprocess_trace(traceDir, traceMapDir, traceFilterDir):
    trace_map = open(traceMapDir, "r")
    pthreadId_to_tid = {}
    varCnt = 0
    varAddrToCnt = {}

    # build a tid -> pthreadId map
    line = trace_map.readline()
    while line:
        if "=" in line:
            strs = line.split("=")
            pthreadId = strs[0].split("(")[1].split(")")[0]
            tid = strs[1].split("(")[1].split(")")[0]
            pthreadId_to_tid[pthreadId] = tid
        line = trace_map.readline()
    trace_map.close()

    print(pthreadId_to_tid)

    # build a tid -> pthreadId map
    traceSaveDir = traceDir + ".temp"
    trace = open(traceDir, "r")
    trace_save = open(traceSaveDir, "w")

    line = trace.readline()
    while line:
        # process instructions
        strs = line.split(":")
        if len(strs) == 2:
            # read / write / atomics

            inst = strs[0]
            line = strs[1].strip()
            instLineNum = inst.split(']')[0].split('[')[1]

            # event only r/w/rw
            strs = line.split("|")
            thread = strs[0]
            op = strs[1]
            target = op.split("(")[1].split(")")[0]
            opCode = op.split("(")[0]

            if(opCode == "z"):
                trace_save.write(thread + "|" + "r(" + op.split("(")[1] + "|" + instLineNum + "\n")
                trace_save.write(thread + "|" + "w(" + op.split("(")[1] + "|" + instLineNum + "\n")
            else:
                if target not in varAddrToCnt:
                    varAddrToCnt[target] = varCnt
                    varCnt += 1
                saveOp = opCode + "(" + str(varAddrToCnt[target]) + ")"
                res = thread + "|" + saveOp + "|" + instLineNum + "\n"
                trace_save.write(res)


        else:
            strs = line.split("|")
            thread = strs[0]
            res = ""
            op = strs[1]
            target = op.split("(")[1].split(")")[0]

            if op.startswith("fork"):
                pthreadId = op.split("(")[1].split(")")[0]
                tid = pthreadId_to_tid[pthreadId]
                res += thread + "|"
                res += "fork(" + tid + ")|"
                res += strs[2]
            elif op.startswith("join"):
                pthreadId = op.split("(")[1].split(")")[0]
                tid = pthreadId_to_tid[pthreadId]
                res += thread + "|"
                res += "join(" + tid + ")|"
                res += strs[2]
            elif op.startswith("free"):
                if target in varAddrToCnt:
                    varAddrToCnt.pop(target)
                line = trace.readline()
                continue
            else:
                res = line
            trace_save.write(res)
        # print(res)

        line = trace.readline()

    trace.close()
    trace_save.close()
    
    print("start filtering!")
    print("source file: " + traceSaveDir)
    print("target file: " + traceFilterDir)

    filterTrace(traceSaveDir, traceFilterDir)
    # os.remove(traceSaveDir)

def filterTrace(sourceFile, targetFile):
    trace = open(sourceFile, "r")
    line = trace.readline()

    varToThreads = {}
    lockToThreads = {}

    while line:
        strs = line.split("|")
        thread = strs[0]
        op = strs[1]
        line = strs[2]
        opcode = op.split("(")[0]
        target = op.split("(")[1].split(")")[0]

        if opcode == "acq" or opcode == "rel":
            if target not in lockToThreads:
                lockToThreads[target] = set()
            lockToThreads[target].add(thread)
        elif opcode == "w" or opcode == "r" or opcode == "z":
            if target not in varToThreads:
                varToThreads[target] = set()
            varToThreads[target].add(thread)
        line = trace.readline()

    trace.close()

    # filter thread local events
    trace = open(sourceFile, "r")
    line = trace.readline()
    trace_save = open(targetFile, "w")

    while line:
        strs = line.split("|")
        thread = strs[0]
        op = strs[1]
        codeLine = strs[2]
        opcode = op.split("(")[0]
        target = op.split("(")[1].split(")")[0]

        if opcode == "acq" or opcode == "rel":
            if len(lockToThreads[target]) > 1:
                trace_save.write(line)
        elif opcode == "w" or opcode == "r" or opcode == "z":
            if len(varToThreads[target]) > 1:
                trace_save.write(line)
        else:
            trace_save.write(line)
        line = trace.readline()
        
    trace.close()
    trace_save.close()


def main():
    traceDir = sys.argv[1]
    traceMapDir = sys.argv[2]
    traceSaveDir = sys.argv[3]
    preprocess_trace(traceDir, traceMapDir, traceSaveDir)

main()
