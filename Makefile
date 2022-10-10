CCFLAGS = -g -D_GNU_SOURCE -I$(INCLUDE) -Werror -pedantic

SRC= src/file_tinyblob.o
INCLUDE=include/
TEST= test.o
CC=gcc
.PHONY: clean


all: clean test

%.o: %.c
	$(CC) $(CCFLAGS) -c $^ -o $@

test: $(TEST) $(SRC)
	gcc $(CCFLAGS) $^ -o $@

clean:
	rm -rf *.o test store_data/*