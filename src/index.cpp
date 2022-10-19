#include "index.h"

#include <cstdint>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>

#include <cassert>

extern struct tiny_blob_handle_t *tb_handle;

int tiny_index::put(char *key, char *value) {
  assert(handle_ != NULL);

  bid_t blob_id = -1;

  // if no such key
  if (lookup_.find(key) == lookup_.end()) {
    uint32_t key_size = strlen(key);
    // TODO:find a solution regarding value size
    uint32_t value_size = strlen(value);

    // Create kv_pair
    // NOTE: can write it into a function
    struct tiny_kv_pair *kv =
        (struct tiny_kv_pair *)malloc(sizeof(struct tiny_kv_pair));
    kv->key_size = key_size;
    kv->value_size = value_size;
    memcpy(&kv->key, &key, key_size);
    memcpy(&kv->value, &value, value_size);
    uint32_t kvsize = sizeof(*kv);
    // RMW
    char *buffer = (char *)malloc(FILE_BLOB_SIZE);
    bid_t available_blob_id = find_next_available_blob(kvsize);
    if (available_blob_id < 0) {
      printf("ERROR: run out of space in blobs");
    }
    if (tb_read_blob(available_blob_id, buffer) < 0) {
      printf("ERROR: reading blob for read-modify-write the new kv-pair\n",
             available_blob_id);
      exit(EXIT_FAILURE);
    }
    // get offset inside blob
    RWLOCK_WRLOCK(tb_handle->lock);
    uint64_t offset = OFFSET_IN_BLOB(available_blob_id);
    tb_handle->table[available_blob_id]->size_used -= kvsize;
    RWLOCK_UNLOCK(tb_handle->lock);

    if (offset == -1) {
      assert(0);
    }
    memcpy(buffer + offset, kv, kvsize);
    if (tb_write_blob(available_blob_id, buffer) < 0) {
      printf("ERROR: Could not write updated blob back\n");
      exit(EXIT_FAILURE);
    }
    // TODO: should free but crrently i get abort
    // free(buffer);
    free(kv);

    // now lets create its metadata for the index
    struct lookup_index_node lnode;
    lnode.tiny_kv_pair_size = kvsize;
    lnode.blob_id = available_blob_id;
    lnode.blob_offset = offset;

    // now insert it into the index's map
    lookup_.emplace(key, lnode);

  } else {
    // if exists
  }

  for (auto pair : lookup_) {
    printf("_%s_:_%d_ | size: %d\n", pair.first, pair.second.tiny_kv_pair_size,
           lookup_.size());
  }
  printf("---\n");
  return 0;
}

char *tiny_index::get(char *key) {
  assert(handle_ != NULL);

  auto found = lookup_.find(key);
  bid_t blob_id = -1;

  // if there is no such key
  if (found == lookup_.end()) {
    return NULL;
  }

  // user should free the buffer after use.
  return NULL;
}

int tiny_index::erase(char *key) { return -1; }

scanner_handle_t tiny_index::scan_init(void) {
  scanner_handle_t scanner;
  return scanner;
}

char *tiny_index::get_scan_key(scanner_handle_t scanner) { return NULL; }

char *tiny_index::get_scan_value(scanner_handle_t scanner) { return NULL; }

int tiny_index::close_scanner(scanner_handle_t scanner) { return -1; }

void tiny_index::persist(char *location) {
  assert(handle_ != NULL);
  persist_unordered_map();
  tb_shutdown();
}

void tiny_index::recover(char *location) {
  tb_init(location);
  handle_ = tb_handle;
  recover_unordered_map();
  printf("allocator blob_meta size %d\n", handle_->bidno);
}

void tiny_index::recover_unordered_map() {
  // check for existent lookup table

  if (tb_handle->table[LOOKUP_TABLE_BLOB_ID] == NULL) {
    // if new index
    // allocate 0 blob
    bid_t blob_id = tb_allocate_blob(HANDLE);
    printf("%lu\n", blob_id);
    assert(blob_id == 0);
    // write parts of tiny_index
    // save blob_ids that should contain the rest of the tiny_index data
  } else {
    // if index exists
    // for every block
    bid_t current_block = LOOKUP_TABLE_BLOB_ID;
    char *buffer = (char *)malloc(FILE_BLOB_SIZE);
    while (current_block != 0) {
      struct lookup_index *lindex =
          (lookup_index *)malloc(sizeof(lookup_index));
      if (tb_read_blob(LOOKUP_TABLE_BLOB_ID, lindex) < 0) {
        printf("ERROR: Could not read index_meta_blob\n");
        exit(EXIT_FAILURE);
      }
      for (int i = 0; i < lindex->map_size; i++) {
        lookup_index_node lnode = lindex->map_data[i];

        struct tiny_kv_pair *kv_pair =
            (struct tiny_kv_pair *)malloc(sizeof(tiny_kv_pair));
        if (tb_read_blob(lnode.blob_id, buffer) < 0) {
          printf("ERROR: recover_unordered_map: Could not read blob\n");
          // handle error
          exit(EXIT_FAILURE);
        }
        memcpy(kv_pair, buffer + lnode.blob_offset, lnode.tiny_kv_pair_size);
        lookup_.emplace((const char *)kv_pair->key, lnode);
        free(kv_pair);
      }
      current_block = lindex->next_blob_id;
    }
    free(buffer);
    //  build unordered map with data from this blob
    //  if end of blob: continue to the next blob(if exists) and repeat
    for (auto pair : lookup_) {
      printf("_%s_:_%d_ | size: %d\n", pair.first, pair.second.blob_offset,
             lookup_.size());
    }
  }
}

void tiny_index::persist_unordered_map() {
  int total_kvs = lookup_.size();
  lookup_index *lindex = (lookup_index *)malloc(sizeof(lookup_index));
  lindex->next_blob_id = 0;
  int i = 0;
  bid_t current_blob_id = LOOKUP_TABLE_BLOB_ID;
  for (std::pair<const char *, lookup_index_node> kv : lookup_) {
    // memcpy(&lindex->map_data[i], &kv.second, sizeof(lookup_index_node));
    lindex->map_data[i] = kv.second;
    i++;
    lindex->map_size = i;
    // if (i >= MAX_LOOKUP_INDEX_NODE_COUNT) {
    //   // lets allocate a new blob for the rest of the index_nodes
    //   bid_t next_blob_id = tb_allocate_blob(HANDLE);
    //   lindex->next_blob_id = next_blob_id;
    //   lindex->map_size = i;
    //   // we ve written so far "i" kv_pairs
    //   total_kvs -= i;
    //   // write the first index nodes
    //   if (tb_write_blob(current_blob_id, lindex) < 0) {
    //     printf("ERROR: could not persist unordered map in blob\n");
    //     exit(EXIT_FAILURE);
    //   }
    //   free(lindex);
    //   // lets allocate a new buffer for the next blob
    //   lindex = (lookup_index *)malloc(sizeof(lookup_index));
    //   lindex->next_blob_id = -1;
    //   // and now write the rest of the keys to the next blob
    //   current_blob_id = next_blob_id;
    //   i = 0;
    // }
  }
  lindex->map_size = i;
  tb_write_blob(current_blob_id, lindex);
  free(lindex);
}

/*

    [ superblock ] [ data ]
                      |
                      \/
                      | blob0 | blob1 | blob2 | blob3 ...
                        ^
                        |
                        /
              tiny_index
            ^
           /
          /
        [ next_blob_ID unordered_map_SIZE unordered_map_DATA ]







*/
