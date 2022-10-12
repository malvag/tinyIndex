CCFLAGS = -g -D_GNU_SOURCE -I$(INCLUDE) -Werror -pedantic
CXXFLAGS = -g -D_GNU_SOURCE -I$(INCLUDE)

FILE_TINYBLOB= src/file_tinyblob.o
BLOCK_TINYBLOB= src/block_tinyblob.o

INCLUDE=include/
TEST= test.o
CC=gcc
CXX=g++
TEST_FILE=test_file.o
TEST_BLOCK=test_block.o

all: clean block_test
.PHONY: clean

src/file_tinyblob.o: src/file_tinyblob.c
	$(CC) $(CCFLAGS) -c $^ -o $@

src/block_tinyblob.o: src/block_tinyblob.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(TEST_BLOCK): test_block.c
	$(CXX) -c $^ $@

$(TEST_FILE): test_file.c
	$(CC) -c $^ -o $@

file_test: $(TEST_FILE) $(FILE_TINYBLOB)
	$(CC) $(CCFLAGS) $^ -o test

block_test: $(TEST_BLOCK) $(BLOCK_TINYBLOB)
	$(CXX) $(CXXFLAGS)  $^ -o test

clean:
	rm -rf src/*.o *.o store_data test
	mkdir store_data
