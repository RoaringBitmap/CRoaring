# minimalist makefile
.SUFFIXES:
#
.SUFFIXES: .cpp .o .c .h
ifeq ($(DEBUG),1) # to debug, build with "make DEBUG=1"
CFLAGS1 = -fPIC  -std=c11 -ggdb -mavx2 -march=native -Wall -Wshadow -Wextra -pedantic
else
CFLAGS1 = -fPIC -std=c11 -O3 -mavx2 -march=native -Wall -Winline  -Wshadow -Wextra -pedantic
endif # debug

ifeq ($(NOAVXTUNING),1) # if you compile with "make NOAVXTUNING=1" you get what the compiler offers!
CFLAGS = $(CFLAGS1)
else # by default we compile for AVX
CFLAGS = $(CFLAGS1) -DUSEAVX
endif # noavx

HEADERS=./include/array_util.h ./include/bitset_util.h ./include/utilasm.h ./include/roaring.h ./include/containers/bitset.h ./include/roaring_array.h ./include/containers/containers.h ./include/containers/convert.h ./include/misc/configreport.h

INCLUDES=-Iinclude  -Iinclude/containers
BENCHINCLUDES=-Ibenchmarks/include


OBJECTS= roaring.o bitset.o roaring_array.o array.o run.o array_util.o  bitset_util.o mixed_intersection.o mixed_union.o convert.o containers.o
TESTEXECUTABLES=unit bitset_container_unit array_container_unit mixed_container_unit run_container_unit toplevel_unit
EXECUTABLES=$(TESTEXECUTABLES) real_bitmaps_benchmark bitset_container_benchmark array_container_benchmark run_container_benchmark
all:  $(EXECUTABLES)


test: $(TESTEXECUTABLES)
	for exe in $(TESTEXECUTABLES) ; do \
		./$$exe ; \
	done

roaring_array.o: ./src/roaring_array.c $(HEADERS)
	$(CC) $(CFLAGS) -c ./src/roaring_array.c $(INCLUDES)

roaring.o: ./src/roaring.c $(HEADERS)
	$(CC) $(CFLAGS) -c ./src/roaring.c $(INCLUDES)

bitset.o: ./src/containers/bitset.c ./include/containers/bitset.h
	$(CC) $(CFLAGS) -c ./src/containers/bitset.c $(INCLUDES)

array.o: ./src/containers/array.c ./include/containers/array.h
	$(CC) $(CFLAGS) -c ./src/containers/array.c $(INCLUDES)


run.o: ./src/containers/run.c ./include/containers/run.h
	$(CC) $(CFLAGS) -c ./src/containers/run.c $(INCLUDES)

convert.o: ./src/containers/convert.c $(wildcard ./include/containers/*.h)
	$(CC) $(CFLAGS) -c ./src/containers/convert.c $(INCLUDES)

mixed_union.o: ./src/containers/mixed_union.c $(wildcard ./include/containers/*.h)
	$(CC) $(CFLAGS) -c ./src/containers/mixed_union.c $(INCLUDES)

mixed_intersection.o: ./src/containers/mixed_intersection.c $(wildcard ./include/containers/*.h)
	$(CC) $(CFLAGS) -c ./src/containers/mixed_intersection.c $(INCLUDES)



containers.o: ./src/containers/containers.c $(wildcard ./include/containers/*.h)
	$(CC) $(CFLAGS) -c ./src/containers/containers.c $(INCLUDES)

bitset_util.o: ./src/bitset_util.c ./include/bitset_util.h
	$(CC) $(CFLAGS) -c ./src/bitset_util.c $(INCLUDES)

array_util.o: ./src/array_util.c ./include/array_util.h
	$(CC) $(CFLAGS) -c ./src/array_util.c $(INCLUDES)

unit: ./tests/unit.c    $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o unit ./tests/unit.c $(INCLUDES)  $(OBJECTS)

bitset_container_unit: ./tests/bitset_container_unit.c    $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o bitset_container_unit ./tests/bitset_container_unit.c $(INCLUDES)  $(OBJECTS)

array_container_unit: ./tests/array_container_unit.c    $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o array_container_unit ./tests/array_container_unit.c $(INCLUDES)  $(OBJECTS)

mixed_container_unit: ./tests/mixed_container_unit.c    $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o mixed_container_unit ./tests/mixed_container_unit.c $(INCLUDES)  $(OBJECTS)

run_container_unit: ./tests/run_container_unit.c    $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o run_container_unit ./tests/run_container_unit.c $(INCLUDES)  $(OBJECTS)

toplevel_unit: ./tests/toplevel_unit.c    $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o toplevel_unit ./tests/toplevel_unit.c $(INCLUDES)  $(OBJECTS)



bitset_container_benchmark: ./benchmarks/bitset_container_benchmark.c ./benchmarks/benchmark.h ./benchmarks/random.h   $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o bitset_container_benchmark ./benchmarks/bitset_container_benchmark.c $(INCLUDES)  $(OBJECTS)


array_container_benchmark: ./benchmarks/array_container_benchmark.c ./benchmarks/benchmark.h ./benchmarks/random.h  $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o array_container_benchmark ./benchmarks/array_container_benchmark.c $(INCLUDES)  $(OBJECTS)

run_container_benchmark: ./benchmarks/run_container_benchmark.c ./benchmarks/benchmark.h ./benchmarks/random.h   $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o run_container_benchmark ./benchmarks/run_container_benchmark.c $(INCLUDES)  $(OBJECTS)

real_bitmaps_benchmark: ./benchmarks/real_bitmaps_benchmark.c ./benchmarks/benchmark.h ./benchmarks/random.h   $(HEADERS) $(OBJECTS)
	$(CC) $(CFLAGS) -o real_bitmaps_benchmark ./benchmarks/real_bitmaps_benchmark.c $(INCLUDES)  $(OBJECTS)


clean:
	rm -f $(EXECUTABLES) $(OBJECTS)
