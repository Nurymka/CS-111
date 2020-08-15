#! /usr/local/cs/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
# 8. wait for lock per operation (ns)
#
# output:
#	lab2_list-1.png ... cost per operation vs threads and iterations
#	lab2_list-2.png ... threads and iterations that run (un-protected) w/o failure
#	lab2_list-3.png ... threads and iterations that run (protected) w/o failure
#	lab2_list-4.png ... cost per operation vs number of threads
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#
#	Early in your implementation, you will not have data for all of the
#	tests, and the later sections may generate errors for missing data.
#

# general plot parameters
set terminal png
set datafile separator ","

# unset the kinky x axis
unset xtics
set xtics

set title "Scalability-1: Throughput of Synchronized Lists"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Throughput (operations/sec)"
set logscale y
set output 'lab2b_1.png'
set key left top
plot \
     "< grep -e 'list-none-m,[1248][264]\\?,1000,1,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
	title '(adjusted) list w/mutex' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-s,[1248][264]\\?,1000,1,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
	title '(adjusted) list w/spin-lock' with linespoints lc rgb 'green'

set title "Scalability-2: Per-opertaion Times for Mutex-Protected List Operations"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Mean time per operation (ns)"
set logscale y
set output 'lab2b_2.png'
set key left top
plot \
     "< grep -e 'list-none-m,[1248][64]\\?,1000,1,' lab2b_list.csv" using ($2):($8) \
	title 'wait for lock' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-m,[1248][64]\\?,1000,1,' lab2b_list.csv" using ($2):($7) \
	title 'completion time' with linespoints lc rgb 'blue'

set title "Scalability-3: Correct Synchronization of Partitioned Lists"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Successful iterations"
set logscale y
set output 'lab2b_3.png'
set key left top
plot \
  "< grep -e 'list-id-none,[148][26]\\?,[1248]6\\?,4' lab2b_list.csv" using ($2):($3) \
  title 'unprotected' with points lc rgb 'red', \
  "< grep -e 'list-id-m,[148][26]\\?,[1248]0,4' lab2b_list.csv" using ($2):($3) \
  title 'Mutex' with points lc rgb 'green', \
  "< grep -e 'list-id-s,[148][26]\\?,[1248]0,4' lab2b_list.csv" using ($2):($3) \
  title 'Spin-Lock' with points lc rgb 'blue'

set title "Scalability-4: Throughput of Mutex-Synchronized Partitioned Lists"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Throughput (operations/sec)"
set logscale y
set output 'lab2b_4.png'
set key left top
plot \
     "< grep -e 'list-none-m,[1248]2\\?,1000,1,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
	title 'lists=1' with linespoints lc rgb 'violet', \
     "< grep -e 'list-none-m,[1248]2\\?,1000,4,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
	title 'lists=4' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-m,[1248]2\\?,1000,8,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
	title 'lists=8' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-m,[1248]2\\?,1000,16,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
	title 'lists=16' with linespoints lc rgb 'orange', \

set title "Scalability-5: Throughput of Spin-Lock-Synchronized Partitioned Lists"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Throughput (operations/sec)"
set logscale y
set output 'lab2b_5.png'
set key left top
plot \
  "< grep -e 'list-none-s,[1248]2\\?,1000,1,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
  title 'lists=1' with linespoints lc rgb 'violet', \
  "< grep -e 'list-none-s,[1248]2\\?,1000,4,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
  title 'lists=4' with linespoints lc rgb 'green', \
  "< grep -e 'list-none-s,[1248]2\\?,1000,8,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
  title 'lists=8' with linespoints lc rgb 'blue', \
  "< grep -e 'list-none-s,[1248]2\\?,1000,16,' lab2b_list.csv" using ($2):(1000000000/(($7)/(($3)/4))) \
  title 'lists=16' with linespoints lc rgb 'orange', \
