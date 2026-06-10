set term png size 1600,800
set output "app-cdfs.png"

@ARG1

set multiplot \
  title "MSE26 Incremental Track Apps" \
  layout 2,3

set xlabel "CPU time (s)"
set ylabel "# instances solved"
set xrange [0:3600]
set key right bottom

set title "adaboost"

plot \
  "ipamiradaboost-cdf.dat" using 2:1 with linespoints linestyle ipamiradaboostl1 title ipamiradaboostn1, \
  "ipamiradaboost-cdf.dat" using 3:1 with linespoints linestyle ipamiradaboostl2 title ipamiradaboostn2, \
  "ipamiradaboost-cdf.dat" using 4:1 with linespoints linestyle ipamiradaboostl3 title ipamiradaboostn3, \
  "ipamiradaboost-cdf.dat" using 5:1 with linespoints linestyle ipamiradaboostl4 title ipamiradaboostn4, \
  "ipamiradaboost-cdf.dat" using 6:1 with linespoints linestyle ipamiradaboostl5 title ipamiradaboostn5

set title "extenf"

plot \
  "ipamirextenf-cdf.dat" using 2:1 with linespoints linestyle ipamirextenfl1 title ipamirextenfn1, \
  "ipamirextenf-cdf.dat" using 3:1 with linespoints linestyle ipamirextenfl2 title ipamirextenfn2, \
  "ipamirextenf-cdf.dat" using 4:1 with linespoints linestyle ipamirextenfl3 title ipamirextenfn3, \
  "ipamirextenf-cdf.dat" using 5:1 with linespoints linestyle ipamirextenfl4 title ipamirextenfn4, \
  "ipamirextenf-cdf.dat" using 6:1 with linespoints linestyle ipamirextenfl5 title ipamirextenfn5

set title "bioptsat"

plot \
  "ipamirbioptsat-cdf.dat" using 2:1 with linespoints linestyle ipamirbioptsatl1 title ipamirbioptsatn1, \
  "ipamirbioptsat-cdf.dat" using 3:1 with linespoints linestyle ipamirbioptsatl2 title ipamirbioptsatn2, \
  "ipamirbioptsat-cdf.dat" using 4:1 with linespoints linestyle ipamirbioptsatl3 title ipamirbioptsatn3, \
  "ipamirbioptsat-cdf.dat" using 5:1 with linespoints linestyle ipamirbioptsatl4 title ipamirbioptsatn4, \
  "ipamirbioptsat-cdf.dat" using 6:1 with linespoints linestyle ipamirbioptsatl5 title ipamirbioptsatn5

set title "wcnfi"

plot \
  "ipamirwcnfi-cdf.dat" using 2:1 with linespoints linestyle ipamirwcnfil1 title ipamirwcnfin1, \
  "ipamirwcnfi-cdf.dat" using 3:1 with linespoints linestyle ipamirwcnfil2 title ipamirwcnfin2, \
  "ipamirwcnfi-cdf.dat" using 4:1 with linespoints linestyle ipamirwcnfil3 title ipamirwcnfin3, \
  "ipamirwcnfi-cdf.dat" using 5:1 with linespoints linestyle ipamirwcnfil4 title ipamirwcnfin4, \
  "ipamirwcnfi-cdf.dat" using 6:1 with linespoints linestyle ipamirwcnfil5 title ipamirwcnfin5

set title "sibyltrace"

plot \
  "ipamirsibyltrace-cdf.dat" using 2:1 with linespoints linestyle ipamirsibyltracel1 title ipamirsibyltracen1, \
  "ipamirsibyltrace-cdf.dat" using 3:1 with linespoints linestyle ipamirsibyltracel2 title ipamirsibyltracen2, \
  "ipamirsibyltrace-cdf.dat" using 4:1 with linespoints linestyle ipamirsibyltracel3 title ipamirsibyltracen3, \
  "ipamirsibyltrace-cdf.dat" using 5:1 with linespoints linestyle ipamirsibyltracel4 title ipamirsibyltracen4, \
  "ipamirsibyltrace-cdf.dat" using 6:1 with linespoints linestyle ipamirsibyltracel5 title ipamirsibyltracen5
