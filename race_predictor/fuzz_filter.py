import sys

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
    traceSaveDir = sys.argv[2]
    filterTrace(traceDir, traceSaveDir)


main()
