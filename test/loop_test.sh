#!/bin/bash -e

# INTERVAL=500
# TOTAL=2500
# SIZE=1024x768

INTERVAL=100
TOTAL=500

# INTERVAL=250
# TOTAL=1250

# INTERVAL=500
# TOTAL=2500

# INTERVAL=2000
# TOTAL=10000

SIZE=1920x1080

while true; do
	echo "----------------------------------------"
	echo "new"
	./test -f /Users/davidl/Desktop/test_full.bmp --headless --destscale $SIZE --interval $INTERVAL --ticks $TOTAL -r DLIR
	echo "benchmark"
	./test -f /Users/davidl/Desktop/test_full.bmp --headless --destscale $SIZE --interval $INTERVAL --ticks $TOTAL -r Bi11
done
