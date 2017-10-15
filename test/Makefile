run_bench: bench
	./bench

run: build
# 	#./test -c
# 	# ./test -r bi2 -r bi3 --cmp test_330.png
# 	# ./test -r bi3 test_full.png
# 	#./test -r bi --headless --interval 10
# 	# ./test -r bi2 --headless --interval 500 --ticks 2500 && ./test -r bi1 --headless --interval 500 --ticks 2500 && ./test -r scalesdl --headless --interval 500 --ticks 2500
# 	# ./test -r bi2 --headless --interval 500 --ticks 2500 test_full.png && ./test -r bi1 --headless --interval 500 --ticks 2500 test_full.png && ./test -r scalesdl --headless --interval 500 --ticks 2500 test_full.png
# 	# ./test -r bi2 -r bi5 --cmp -f test_330.png --destscale 448x448
# 	# ./test -r bi2 -r bi2b --cmp -f test_330.png --destscale 448x448
	# ./test -r bi2 -r bi10 --cmp --destscale 448x448
	# ./test -r bi2 -f test_full.png --destscale 1024x768 -r src
	# ./test -r bi11 -r DLIR --cmp --destscale 448x448

	# ./test -r src -r DLIR -f test_330.png --destscale 660x660
	# ./test -r src -r DLIR -f test_330.png --destscale 450x450
	# ./test -r src -r DLIR -f test_330.png --destscale 330x330
	./test -r src -r DLIR -f test_full.png --destscale 640x480
	# ./test -r src -r DLIR --destscale 330x330 -c

build: DLTest.cpp DLTest.h main.cpp Makefile # x86
	#emcc test.cpp -O2 -o test.html -I .. -std=c++11 $(shell ./dltest-config emscripten)

	# clang++ test.cpp -g -o test -D DEBUG -O0 -I .. -std=c++11 $(shell ./dltest-config)
	# clang++ -fsanitize=address main.cpp DLTest.cpp ../DL_ImageResize.cpp x86_3.o x86_4.o x86_5.o -g -o test -Ofast -msse4.1 -I .. -arch i386 -std=c++11 $(shell ./dltest-config)
	clang++ -fsanitize=address main.cpp DLTest.cpp ../DL_ImageResize.cpp -g -o test -Ofast -msse4.1 -I .. -arch i386 -std=c++11 $(shell ./dltest-config)
	# clang++ main.cpp DLTest.cpp x86.o -g -o test -O0 -I .. -arch i386 -std=c++14 $(shell ./dltest-config)


BENCH_SRCS := bench_main.cpp $(wildcard external/benchmark/src/*.cc)

bench: $(BENCH_SRCS) Makefile
	clang++ -DHAVE_STD_REGEX -DNDEBUG -fsanitize=address $(BENCH_SRCS) -o bench -Ofast -msse4.1 -std=c++11 -I external/benchmark/include
	# clang++ -DHAVE_STD_REGEX -fsanitize=address $(BENCH_SRCS) -o bench -g -msse4.1 -std=c++11 -I external/benchmark/include

# x86: x86_3.nasm x86_4.nasm x86_5.nasm
# 	nasm -f macho x86_3.nasm
# 	nasm -f macho x86_4.nasm
# 	nasm -f macho x86_5.nasm

# run: test
# 	./test

clean:
	rm -f test
	rm -f bench
