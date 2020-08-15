#!/bin/bash

LOGNAME="test_logfile"
TEST_OUTPUT="test_output"
TEST_STDOUT="test_stdout"

function cleanup {
  rm -f $LOGNAME $TEST_OUTPUT $TEST_STDOUT
}

function errorExit {
  cleanup
  exit 1
}

echo "testing ./lab4b..."

echo "testing basic behaviour:"
./lab4b --period=2 --scale=C --log=$LOGNAME > $TEST_STDOUT <<-EOF
SCALE=F
PERIOD=1
STOP
START
LOG JAJA
hahaha
OFF
EOF

echo " - testing exit code"
if [ $? -ne 0 ]
then
  echo "FAIL: didn't exit with code 0"
  errorExit
fi

echo " - testing writing to log"
if [ ! -s $LOGNAME ]
then
  echo "FAIL: didn't write anything to log"
  errorExit
fi

echo " - testing writing to stdout"
if [ ! -s $TEST_STDOUT ]
then
  echo "FAIL: didn't write anything to stdout"
  errorExit
fi

echo " - testing OFF command"
cat $LOGNAME | grep -e "SHUTDOWN" > $TEST_OUTPUT
if [[ ! -s $TEST_OUTPUT ]]
then
  echo "FAIL: didn't output SHUTDOWN before exiting"
  errorExit
fi

echo " - testing reporting format"
cat $LOGNAME | grep -e "[0-9][0-9]:[0-9][0-9]:[0-9][0-9] [0-9]\+.[0-9]" > $TEST_OUTPUT
if [[ ! -s $TEST_OUTPUT ]]
then
  echo "FAIL: incorrect time reporting format"
  errorExit
fi

echo " - testing LOG <line>"
cat $LOGNAME | grep -e "LOG JAJA" > $TEST_OUTPUT
if [[ ! -s $TEST_OUTPUT ]]
then
  echo "FAIL: LOG <line> command didn't log"
  errorExit
fi

echo " - testing invalid command"
cat $LOGNAME | grep -e "hahaha" > $TEST_OUTPUT
if [[ -s $TEST_OUTPUT ]]
then
  echo "FAIL: invalid command was logged"
  errorExit
fi

cleanup
echo
echo "ALL OK!"
exit 0
