#!/bin/bash -e

MEASURE=500
TOTAL=2500

while true; do
	echo "----------------------------------------"
	echo "new"
	./test -f /Users/davidl/Desktop/test_full.bmp -h -s 1024x768 -n $MEASURE -t $TOTAL -r bi6
	echo "benchmark"
	./test -f /Users/davidl/Desktop/test_full.bmp -h -s 1024x768 -n $MEASURE -t $TOTAL -r bi2
done
