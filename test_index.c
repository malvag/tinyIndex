#include <assert.h>
#include <stdlib.h>

#include "allocator.h"
#include "index.h"
#include "string.h"
#include <iostream>
int main(int argc, char **argv) {
  tiny_index blob_idx;
  blob_idx.recover(argv[1]);
  for (int i = 0; i < 5; i++) {
    blob_idx.put("malvag", "data");
  }
  blob_idx.persist(argv[1]);
  return 0;
}
