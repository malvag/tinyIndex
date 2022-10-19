#include <assert.h>
#include <stdlib.h>

#include <iostream>
#include <fmt/core.h>
#include "allocator.h"
#include "index.h"
#include "string.h"
int main(int argc, char **argv) {

  char key[11]; // enough to hold all numbers up to 64-bits
  char val[9];
    tiny_index blob_idx;
    blob_idx.recover(argv[1]);
    for (int i = 0; i < 1024; i++) {
        sprintf(key, "malvag%d", i);
        sprintf(val, "data%d", i);
        blob_idx.put(key, val);
        // std::cout << blob_idx.get("malvag") << '\n';
        // std::cout << blob_idx.handle_->bidno << '\n';
    }
    blob_idx.persist(argv[1]);
    return 0;
}
