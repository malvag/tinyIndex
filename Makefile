CXXFLAGS = -g  -D_GNU_SOURCE -I$(INCLUDE) #-fsanitize=address


INCLUDE=include/
TEST= test.o
CXX=g++
# TEST=test_log.o
TEST=test_block.o

LIBRARY_OBJ= src/tiny_allocator.o src/tiny_log.o src/tiny_scanner.o  src/tiny_index.o 
LIBRARY= libtiny.a

all: clean library
.PHONY: clean library

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS)  -c $^ -o $@

test: $(TEST) $(LIBRARY_OBJ)
	$(CXX) $(CXXFLAGS)  $^ -o test

library: $(LIBRARY_OBJ)
	ar rcs $(LIBRARY) $(LIBRARY_OBJ)

clean:
	rm -rf src/*.o *.o *.a store_data test
	mkdir store_data
