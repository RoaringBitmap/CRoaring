# minimalist makefile
.SUFFIXES:
#
.SUFFIXES: .cpp .o .c .h
ifeq ($(DEBUG),1)
CFLAGS = -fPIC  -std=c99 -ggdb -msse4.1  -mavx  -mavx2 -march=native -Wall -Wextra -pedantic
else
CFLAGS = -fPIC -std=c99 -O3 -msse4.1 -mavx  -mavx2  -march=native -Wall -Wextra -pedantic
endif # debug

HEADERS=./include/roaring.h ./include/containers/bitset.h

INCLUDES=-Iinclude  -Iinclude/containers
BENCHINCLUDES=-Ibenchmarks/include 


OBJECTS= roaring.o bitset.o
TESTEXECUTABLES=unit bitset_container_unit
EXECUTABLES=$(TESTEXECUTABLES) bitset_container_benchmark
all:  $(EXECUTABLES) 

test:
	./unit
	./bitset_container_unit


roaring.o: ./src/roaring.c $(HEADERS)
	$(CC) $(CFLAGS) -c ./src/roaring.c 

bitset.o: ./src/containers/bitset.c ./include/containers/bitset.h
	$(CC) $(CFLAGS) -c ./src/containers/bitset.c $(INCLUDES)

unit: ./tests/unit.c    $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o unit ./tests/unit.c $(INCLUDES)  $(OBJECTS)

bitset_container_unit: ./tests/bitset_container_unit.c    $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o bitset_container_unit ./tests/bitset_container_unit.c $(INCLUDES)  $(OBJECTS)


bitset_container_benchmark: ./benchmarks/bitset_container_benchmark.c ./benchmarks/benchmark.h   $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o bitset_container_benchmark ./benchmarks/bitset_container_benchmark.c $(INCLUDES)  $(OBJECTS)


clean:
	rm -f $(EXECUTABLES) $(OBJECTS)
