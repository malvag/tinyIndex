#pragma once

#include <memory>
#include <unordered_map>

#include "allocator.h"
#include "scanner.h"

#define LOOKUP_TABLE_BLOB_ID 0
#define OFFSET_IN_BLOB(bid) FILE_BLOB_SIZE - tb_handle->table[bid]->size_used

struct tiny_kv_pair {
  uint32_t key_size;
  const char *key;

  uint32_t value_size;
  const char *value;
} __attribute__((__packed__));

struct lookup_index_node {
  uint32_t tiny_kv_pair_size; // go-to-allocated-blobs-> read first 4bytes
  bid_t blob_id;
  uint64_t blob_offset;
} __attribute__((__packed__));

struct lookup_index {
  bid_t next_blob_id;
  uint32_t map_size;
  struct lookup_index_node map_data[MAX_LOOKUP_INDEX_NODE_COUNT];
} __attribute__((__packed__));

// typedef struct blob blob_t;
class tiny_index {
private:
  void persist_unordered_map();
  void recover_unordered_map();

public:
  tiny_blob_handle_t *handle_ = nullptr;
  std::unordered_map<const char *, lookup_index_node> lookup_;

  // Return 0 on success -1 on failure.
  int put(char *key, char *value);

  // Return a buffer filled with the value if key is
  // found, null if key is not found.
  char *get(char *key);

  // Return 0 on success -1 on failure.
  int erase(char *key);

  // Initialize a scanner return 0 on success -1 on failure.
  scanner_handle_t scan_init(void);

  // Get a pointer to the current key for this scanner.
  char *get_scan_key(scanner_handle_t scanner);

  // Get a pointer to the current value for this scanner.
  char *get_scan_value(scanner_handle_t scanner);

  // Release the resources for the scanner.
  int close_scanner(scanner_handle_t scanner);

  // Store all the key values on the location path.
  void persist(char *location);

  // Restore all the key values stored on the
  // location path.
  void recover(char *location);
};
