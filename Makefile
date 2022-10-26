CXXFLAGS = -g -D_GNU_SOURCE -I$(INCLUDE)# -fsanitize=address


INCLUDE=include/
TEST= test.o
CXX=g++
TEST_BLOCK=test_block.o

LIBRARY= src/allocator.o src/index.o

all: clean test
.PHONY: clean library

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $^ -o $@

test: $(TEST_BLOCK) $(LIBRARY)
	$(CXX) $(CXXFLAGS)  $^ -o test

clean:
	rm -rf src/*.o *.o store_data test
	mkdir store_data
