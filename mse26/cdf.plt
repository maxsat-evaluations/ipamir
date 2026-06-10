set term png size 800,800
set output "cdf.png"

set xlabel "CPU time (s)"
set ylabel "# instances solved"
set xrange [0:3600]

set title "MSE26 Incremental Track"

set key right bottom

plot \
  "cdf.dat" using 2:1 with linespoints linestyle 1 title ARG1, \
  "cdf.dat" using 3:1 with linespoints linestyle 2 title ARG2, \
  "cdf.dat" using 4:1 with linespoints linestyle 3 title ARG3, \
  "cdf.dat" using 5:1 with linespoints linestyle 4 title ARG4, \
  "cdf.dat" using 6:1 with linespoints linestyle 5 title ARG5
