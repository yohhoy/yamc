#!/bin/sh
DATFILE="perf_rwlock.dat"
PNGFILE="perf_rwlock.png"

echo "input: ${DATFILE}"
gnuplot << EOT
set terminal png medium
set output "${PNGFILE}"

set title "rwlock contention"
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
     "${DATFILE}" index 4 using 1:3   with lines     lt 5 title "PhaseFair/WriteLock", \
     "${DATFILE}" index 4 using 1:3:4 with errorbars lt 5 notitle, \
     "${DATFILE}" index 4 using 1:7   with lines     lt 6 title "PhaseFair/ReadLock", \
     "${DATFILE}" index 4 using 1:7:8 with errorbars lt 6 notitle,
EOT
echo "output: ${PNGFILE}"
