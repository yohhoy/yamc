#!/bin/sh
DATFILE="perf_rwlock.dat"
echo "input: ${DATFILE}"

if [ ! -f ${DATFILE} ]; then
  echo "file not found.\nrun \"perf_rwlock > ${DATFILE}\""
  exit 1
fi


PNGFILE="perf_rwlock.png"
echo "output: ${PNGFILE}"
gnuplot << EOT
set terminal png medium
set output "${PNGFILE}"

set title "rwlock throughput"
set xlabel "writer threads"
set ylabel "ops/sec/thread"
set xrange [0.5:9.5]
set yrange [0:]

plot "${DATFILE}" index 2 using 1:3   with lines     lt 1 title "ReaderPerfer/WriteLock", \
     "${DATFILE}" index 2 using 1:3:4 with errorbars lt 1 notitle, \
     "${DATFILE}" index 2 using 1:7   with lines     lt 2 title "ReaderPerfer/ReadLock", \
     "${DATFILE}" index 2 using 1:7:8 with errorbars lt 2 notitle, \
     "${DATFILE}" index 3 using 1:3   with lines     lt 3 title "WriterPerfer/WriteLock", \
     "${DATFILE}" index 3 using 1:3:4 with errorbars lt 3 notitle, \
     "${DATFILE}" index 3 using 1:7   with lines     lt 4 title "WriterPerfer/ReadLock", \
     "${DATFILE}" index 3 using 1:7:8 with errorbars lt 4 notitle, \
     "${DATFILE}" index 4 using 1:3   with lines     lt 5 title "TaskFair/WriteLock", \
     "${DATFILE}" index 4 using 1:3:4 with errorbars lt 5 notitle, \
     "${DATFILE}" index 4 using 1:7   with lines     lt 6 title "TaskFair/ReadLock", \
     "${DATFILE}" index 4 using 1:7:8 with errorbars lt 6 notitle, \
     "${DATFILE}" index 5 using 1:3   with lines     lt 7 title "PhaseFair/WriteLock", \
     "${DATFILE}" index 5 using 1:3:4 with errorbars lt 7 notitle, \
     "${DATFILE}" index 5 using 1:7   with lines     lt 8 title "PhaseFair/ReadLock", \
     "${DATFILE}" index 5 using 1:7:8 with errorbars lt 8 notitle,
EOT


PNGFILE="perf_rwlock-unfair.png"
echo "output: ${PNGFILE}"
gnuplot << EOT
set terminal png medium
set output "${PNGFILE}"

set title "rwlock throughput"
set xlabel "writer threads"
set ylabel "ops/sec/thread"
set xrange [0.5:9.5]
set yrange [0:]

plot "${DATFILE}" index 2 using 1:3 with linespoints lt 1 title "ReaderPerfer/WriteLock", \
     "${DATFILE}" index 2 using 1:7 with linespoints lt 2 title "ReaderPerfer/ReadLock", \
     "${DATFILE}" index 3 using 1:3 with linespoints lt 3 title "WriterPerfer/WriteLock", \
     "${DATFILE}" index 3 using 1:7 with linespoints lt 4 title "WriterPerfer/ReadLock",
EOT


PNGFILE="perf_rwlock-fair.png"
echo "output: ${PNGFILE}"
gnuplot << EOT
set terminal png medium
set output "${PNGFILE}"

set title "rwlock throughput"
set xlabel "writer threads"
set ylabel "ops/sec/thread"
set xrange [0.5:9.5]
set yrange [0:]

plot "${DATFILE}" index 4 using 1:3 with linespoints lt 5 title "TaskFair/WriteLock", \
     "${DATFILE}" index 4 using 1:7 with linespoints lt 6 title "TaskFair/ReadLock", \
     "${DATFILE}" index 5 using 1:3 with linespoints lt 7 title "PhaseFair/WriteLock", \
     "${DATFILE}" index 5 using 1:7 with linespoints lt 8 title "PhaseFair/ReadLock",
EOT
