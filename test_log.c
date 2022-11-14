#include "string.h"
#include "tiny_log.h"
#include "tiny_index.h"
#include <assert.h>
#include <fmt/core.h>
#include <iostream>
#include <stdlib.h>
#include <string>

int main(int argc, char **argv) {
  log_handle* wal = new log_handle(argv[1]);
  for (int i = 0; i < 100; i++) {
    tiny_kv_pair *kv = (tiny_kv_pair *)malloc(sizeof(tiny_kv_pair));
    std::string key = std::string("malvag") + std::to_string(i);
    std::string value =  std::string("adata") + std::to_string(i);
    kv->key = (char*)key.c_str();
    kv->key_size = key.length() + 1;
    kv->value = (char*)value.c_str();
    kv->value_size = value.length() + 1;
    wal->append(kv);
    // blob_idx.get()
    // std::cout << blob_idx.get("malvag") << '\n';
    // std::cout << blob_idx.handle_->bidno << '\n';
  }
  // wal->replay();

  // wal->truncate();

  return 0;
}
