CXXFLAGS = -g  -D_GNU_SOURCE -I$(INCLUDE)# -fsanitize=address


INCLUDE=include/
TEST= test.o
CXX=g++
TEST_BLOCK=test_block.o

LIBRARY_OBJ= src/allocator.o src/scanner.o  src/index.o 
LIBRARY= libtiny.a

all: clean test
.PHONY: clean library

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS)  -c $^ -o $@

test: $(TEST_BLOCK) $(LIBRARY_OBJ)
	$(CXX) $(CXXFLAGS)  $^ -o test

library: $(LIBRARY_OBJ)
	ar rcs $(LIBRARY) $(LIBRARY_OBJ)

clean:
	rm -rf src/*.o *.o *.a store_data test
	mkdir store_data
