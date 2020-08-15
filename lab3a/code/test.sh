#!/bin/bash
INPUT="EXT2_test.img"
ORIG="ext2results_ang.txt"
# INPUT="trivial.img"
# ORIG="trivial.csv"

./lab3a $INPUT > t_output
sort t_output > t_soutput
sort $ORIG > t_soriginal
diff t_soutput t_soriginal
rm -f t_output t_soutput t_soriginal
