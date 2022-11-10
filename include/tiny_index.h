#pragma once

#include "tiny_allocator.h"
#include "tiny_log.h"
#include <memory>
#include <unordered_map>

#define LOOKUP_TABLE_BLOB_ID 0
#define OFFSET_IN_BLOB(bid) FILE_BLOB_SIZE - tb_handle->table[bid]->size_used
#define DEBUG 0

struct tiny_kv_pair {
  uint32_t key_size;
  uint32_t value_size;
  char *key;
  char *value;
} __attribute__((__packed__));

struct lookup_index_node {
  bid_t blob_id;
  uint64_t blob_offset;
  uint32_t tiny_kv_pair_size; // go-to-allocated-blobs-> read first 4bytes
} __attribute__((__packed__));

struct lookup_index {
  struct lookup_index_node map_data[MAX_LOOKUP_INDEX_NODE_COUNT];
  bid_t next_blob_id;
  uint32_t map_size;
} __attribute__((__packed__));
class log_handle_t;
class scanner_handle_t;
// typedef struct blob blob_t;
class tiny_index {
private:
  void persist_unordered_map();
  void recover_unordered_map();

public:
  tiny_blob_handle_t *handle_ = nullptr;
  log_handle_t *log_handle_ = nullptr;
  std::unordered_map<std::string, lookup_index_node> lookup_;

  // Return 0 on success -1 on failure.
  int put(char *key, char *value);

  // Return a buffer filled with the value if key is
  // found, null if key is not found.
  char *get(char *key);

  // Return 0 on success -1 on failure.
  int erase(char *key);

  // Initialize a scanner, return a pointer to a scanner handle on success NULL
  // on failure.
  scanner_handle_t *scan_init(void);

  // Iterate to the next KV, return 0 if handle moved forward or -1 if it
  // reached the end.
  int get_next(scanner_handle_t *scanner);

  // Get a pointer to the current key for this scanner.
  char *get_scan_key(scanner_handle_t *scanner);

  // Get a pointer to the current value for this scanner.
  char *get_scan_value(scanner_handle_t *scanner);

  // Release the resources for the scanner.
  int close_scanner(scanner_handle_t *scanner);

  // Store all the key values on the location path.
  void persist(char *location);

  // Restore all the key values stored on the
  // location path.
  void recover(char *location);
};
