#!/bin/sh

if [ ! -x ./test-driver ]
then
	echo 'Missing test-driver!'
	exit 1
fi

passed=0
failed=0
for input in tests/*.in
do
	name=`basename "$input" .in`
	if ./test-driver <"$input" | diff - "tests/${name}.ref"
	then
		echo "$name passed."
		passed=`expr $passed + 1`
	else
		echo "$name FAILED!"
		failed=`expr $failed + 1`
	fi
done

if [ $failed = 0 ]
then
	echo "All test cases passed."
	exit 0
else
	echo "$passed test cases passed.  $failed test cases failed!"
	exit 1
fi
