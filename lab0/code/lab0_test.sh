#!/bin/bash
# NAME: Nurym Kudaibergen
# EMAIL: nurim98@gmail.com
# ID: 404648263

EXEC="./lab0"

test_exitcode () {
  EXIT_CODE=$?
  if [[ "$EXIT_CODE" -ne "$1" ]]; then
    echo "FAIL: must exit with code $1 (is $EXIT_CODE)"
    exit 1
  fi
}

test_result () {
  if [[ "$1" != "$2" ]]; then
    echo "FAIL: the output doesn't match the input"
    exit 1
  fi
}

echo "...testing copy 'hello world' from stdin to stdout"
TEST1_STR='hello world'
TEST1_OUT=$(echo $TEST1_STR | $EXEC)
test_exitcode 0
test_result "$TEST1_STR" "$TEST1_OUT"

echo "...testing --input option"
TEST2_STR=`cat lab0.c`
TEST2_OUT=$($EXEC --input=lab0.c)
test_exitcode 0
test_result "$TEST2_STR" "$TEST2_OUT"

echo "...testing --input option, unopenable file"
touch unopenable.txt
chmod u-r unopenable.txt
$EXEC --input=unopenable.txt 2> /dev/null
test_exitcode 2
rm -f unopenable.txt

echo "...testing --input option, nonexistent file"
$EXEC --input=nonexistent 2> /dev/null
test_exitcode 2

echo "...testing --output option"
TEST4_STR="i should become an output"
echo $TEST4_STR | $EXEC --output=lab0_output
test_exitcode 0
TEST4_OUT=`cat lab0_output`
rm -f lab0_output
test_result "$TEST4_STR" "$TEST4_OUT"

echo "...testing --output option, unwriteable file"
touch unwriteable.txt
chmod u-w unwriteable.txt
$EXEC --output=unwriteable.txt 2> /dev/null
test_exitcode 3
rm -f unwriteable.txt

echo "...testing combined --input and --output"
TEST6_STR=`cat lab0.c`
$EXEC --input=lab0.c --output=lab0_output
test_exitcode 0
TEST6_OUT=`cat lab0_output`
test_result "$TEST6_STR" "$TEST6_OUT"
rm -f lab0_output

echo "...testing segmentation fault"
$EXEC --segfault
test_exitcode 139

echo "...testing segfault catch"
$EXEC --segfault --catch 2> /dev/null
test_exitcode 4

echo "...testing unrecognized arguments"
$EXEC --hi 2> /dev/null
test_exitcode 1

echo "ALL TESTS PASSED"
