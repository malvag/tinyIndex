#include "include/allocator.h"
#include "include/index.h"
#include "string.h"
#include <assert.h>
#include <fmt/core.h>
#include <iostream>
#include <stdlib.h>
#include <string>

int main(int argc, char **argv) {

  std::string key;
  std::string value;
  tiny_index blob_idx;
  blob_idx.recover(argv[1]);
  for (int i = 0; i < 10000; i++) {
    key = "malvag" + std::to_string(i);
    value = "adata" + std::to_string(i);
    blob_idx.put((char *)key.c_str(), (char *)value.c_str());
    // blob_idx.get()
    // std::cout << blob_idx.get("malvag") << '\n';
    // std::cout << blob_idx.handle_->bidno << '\n';
  }

  char *value_buffer = NULL;

  for (int i = 0; i < 1000; i++) {
    key = "malvag" + std::to_string(i);
    value_buffer = blob_idx.get((char *)key.c_str());
    std::cout << value_buffer << '\n';
    if (value_buffer != NULL)
      free(value_buffer);
  }

  scanner_handle_t *scanr = blob_idx.scan_init();
  int i = 0;
  while (!blob_idx.get_next(scanr)) {
    char *out = blob_idx.get_scan_key(scanr);
    // if (out)
    printf("[%d] %s\n", i++, out);
  }
  blob_idx.close_scanner(scanr);
  blob_idx.persist(argv[1]);

  return 0;
}
