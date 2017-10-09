run: build
# 	#./test -c
# 	# ./test -r bi2 -r bi3 --cmp test_330.bmp
# 	# ./test -r bi3 test_full.bmp
# 	#./test -r bi -h -n 10
# 	# ./test -r bi2 -h -n 500 -t 2500 && ./test -r bi1 -h -n 500 -t 2500 && ./test -r scalesdl -h -n 500 -t 2500
# 	# ./test -r bi2 -h -n 500 -t 2500 test_full.bmp && ./test -r bi1 -h -n 500 -t 2500 test_full.bmp && ./test -r scalesdl -h -n 500 -t 2500 test_full.bmp
# 	# ./test -r bi2 -r bi5 --cmp -f test_330.bmp -s 448x448
# 	# ./test -r bi2 -r bi2b --cmp -f test_330.bmp -s 448x448
	# ./test -r bi2 -r bi10 --cmp -s 448x448
	# ./test -r bi2 -f test_full.bmp -s 1024x768 -r src
	# ./test -r bi11 -r DLIR --cmp -s 448x448

	./test -r src -r DLIR -f test_330.bmp -s 660x660
	# ./test -r src -r DLIR -f test_330.bmp -s 450x450
	# ./test -r src -r DLIR -f test_330.bmp -s 330x330
	# ./test -r src -r DLIR -s 330x330 -c

build: DLTest.cpp DLTest.h main.cpp Makefile x86
	#emcc test.cpp -O2 -o test.html -I .. -std=c++11 $(shell ./dltest-config emscripten)

	# clang++ test.cpp -g -o test -D DEBUG -O0 -I .. -std=c++11 $(shell ./dltest-config)
	clang++ -fsanitize=address main.cpp DLTest.cpp ../DL_ImageResize.cpp x86_3.o x86_4.o x86_5.o -g -o test -Ofast -msse4.1 -I .. -arch i386 -std=c++14 $(shell ./dltest-config)
	# clang++ main.cpp DLTest.cpp x86.o -g -o test -O0 -I .. -arch i386 -std=c++14 $(shell ./dltest-config)

x86: x86_3.nasm x86_4.nasm x86_5.nasm
	nasm -f macho x86_3.nasm
	nasm -f macho x86_4.nasm
	nasm -f macho x86_5.nasm

# run: test
# 	./test

clean:
	rm -f test
