#!/bin/bash -e

# MEASURE=500
# TOTAL=2500
# SIZE=1024x768

# MEASURE=250
# TOTAL=1250

MEASURE=500
TOTAL=2500

# MEASURE=2000
# TOTAL=10000

SIZE=1920x1080

while true; do
	echo "----------------------------------------"
	echo "new"
	./test -f /Users/davidl/Desktop/test_full.bmp -h -s $SIZE -n $MEASURE -t $TOTAL -r bi11
	echo "benchmark"
	./test -f /Users/davidl/Desktop/test_full.bmp -h -s $SIZE -n $MEASURE -t $TOTAL -r bi10
done
