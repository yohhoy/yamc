#!/bin/sh
DATAFILE="perf_rwlock.dat"
PNGFILE="perf_rwlock.png"

gnuplot << EOT
set terminal png
set output "${PNGFILE}"
set xlabel "writer threads"
set ylabel "ops/sec/thread"
set style data linespoints
plot "${DATAFILE}" index 0 using 1:3 title "ReaderPerfer/WriteLock", \
     "${DATAFILE}" index 0 using 1:6 title "ReaderPrefer/ReadLock", \
     "${DATAFILE}" index 1 using 1:3 title "WriterPerfer/WriteLock", \
     "${DATAFILE}" index 1 using 1:6 title "WriterPerfer/ReadLock", \
     "${DATAFILE}" index 2 using 1:3 title "PhaseFair/WriteLock", \
     "${DATAFILE}" index 2 using 1:6 title "PhaseFair/ReadLock",
EOT

echo "output: ${PNGFILE}"
