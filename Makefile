CCFLAGS = -g -D_GNU_SOURCE -Werror -pedantic

TEST= test.o
CC=gcc
.PHONY: clean


all: clean test

%.o: %.c
	$(CC) $(CCFLAGS) -c $^ -o $@

test: $(TEST) file_tinyblob.o
	gcc $(CCFLAGS) $^ -o $@

clean:
	rm -rf *.o test store_data/*