#include "tiny_index.h"
#include "string.h"
#include <assert.h>
#include <fmt/core.h>
#include <iostream>
#include <stdlib.h>
#include <string>

int main(int argc, char **argv) {

  std::string key;
  std::string value;
  tiny_index blob_idx, second_idx;
  blob_idx.recover(argv[1]);
  for (int i = 0; i < 1000; i++) {
    key = "malvag" + std::to_string(i);
    value = "adata" + std::to_string(i);
    blob_idx.put((char *)key.c_str(), (char *)value.c_str());
    // blob_idx.get()
    // std::cout << blob_idx.get("malvag") << '\n';
    // std::cout << blob_idx.handle_->bidno << '\n';
  }

  char *value_buffer = NULL;
  blob_idx.persist(argv[1]);


  second_idx.recover(argv[1]);

  for (int i = 0; i < 1000; i++) {
    key = "malvag" + std::to_string(i);
    value_buffer = second_idx.get((char *)key.c_str());
    
    std::cout << value_buffer << '\n';
    if (value_buffer != NULL)
      free(value_buffer);
  }

  scanner_handle_t *scanr = second_idx.scan_init();
  int i = 0;
  while (!second_idx.get_next(scanr)) {
    char *out = second_idx.get_scan_key(scanr);
    printf("[%d] %s\n", i++, out);
    if (out)
      free(out);
  }
  second_idx.close_scanner(scanr);
  second_idx.persist(argv[1]);

  return 0;
}
