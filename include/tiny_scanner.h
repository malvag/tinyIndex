#pragma once

#define FIRST_DATA_BLOB 1
#include "tiny_index.h"
#include <map>

class scanner_handle_t {
public:
  scanner_handle_t(
      std::unordered_map<std::string, struct lookup_index_node> map);
  int go_to_next();
  ~scanner_handle_t();
  char *get_key();
  char *get_value();

private:
  char *opened_blob_buffer_;
  bid_t opened_blob_;
  std::map<std::string, struct lookup_index_node> *map_;
  std::map<std::string, struct lookup_index_node>::iterator iterator_;
};
