#include "index.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

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
    kv->key = strdup(key);
    kv->value = strdup(value);

    uint32_t true_kvsize = sizeof(kv->key_size) + kv->key_size +
                           sizeof(kv->value_size) + kv->value_size;
    // RMW
    char *buffer = (char *)memalign(FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);
    bid_t available_blob_id = find_next_available_blob(true_kvsize);
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
    tb_handle->table[available_blob_id]->size_used -= true_kvsize;
    RWLOCK_UNLOCK(tb_handle->lock);

    // printf("Written kvpair with key %s with size %lu, in blob %d, with offset
    // "
    //      "%lu\n",
    //    kv->key, true_kvsize, available_blob_id, offset);

    memcpy(buffer + offset, &kv->key_size, sizeof(kv->key_size));

    // printf("with key_size %u\n", *(uint32_t *)(buffer + offset));
    memcpy(buffer + offset + sizeof(kv->key_size), &kv->value_size,
           sizeof(kv->value_size));

    // printf("with value_size %u\n", *(uint32_t *)(buffer + offset + 4));
    memcpy(buffer + offset + sizeof(kv->key_size) + sizeof(kv->value_size),
           kv->key, kv->key_size);

    // printf("with key %s\n", buffer + offset + 4 + 4);
    memcpy(buffer + offset + sizeof(kv->key_size) + kv->key_size +
               sizeof(kv->value_size),
           kv->value, kv->value_size);
    // printf("with value %s\n", buffer + offset + 4 + 4 + kv->key_size);
    if (tb_write_blob(available_blob_id, buffer) < 0) {
      printf("ERROR: Could not write updated blob back\n");
      exit(EXIT_FAILURE);
    }
    // TODO: should free but crrently i get abort
    free(buffer);
    buffer = NULL;
    free(kv);
    kv = NULL;

    // now lets create its metadata for the index
    struct lookup_index_node lnode;
    lnode.tiny_kv_pair_size = true_kvsize;
    lnode.blob_id = available_blob_id;
    lnode.blob_offset = offset;

    // now insert it into the index's map
    lookup_.emplace(std::string(key), lnode);

  } else {
    // if exists
  }

  // printf("---\n");
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
  printf("Persisted %d keys in %d blobs\n", lookup_.size(), tb_handle->bidno);
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
    printf("Allocated new blob for index's metadata %lu\n", blob_id);
    assert(blob_id == 0);
    // write parts of tiny_index
    // save blob_ids that should contain the rest of the tiny_index data
  } else {
    // if index exists
    // for every block
    bid_t current_block = LOOKUP_TABLE_BLOB_ID;
    char *buffer = (char *)malloc(FILE_BLOB_SIZE);
    int first_block = 1;
    while (current_block || first_block) {
      struct lookup_index *lindex =
          (lookup_index *)memalign(FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);
      if (tb_read_blob(current_block, lindex) < 0) {
        printf("ERROR: Could not read index_meta_blob\n");
        exit(EXIT_FAILURE);
      }
      int i = 0;
      for (int i = 0; i < lindex->map_size; i++) {
        auto pair = lindex->map_data[i];
        if (DEBUG)
          printf(
              "Recovered index for kv in blob %lu by offset %lu | size: %lu\n",
              pair.blob_id, pair.blob_offset, pair.tiny_kv_pair_size);
      }
      printf("Recovered %d indices from blob %u\n", i, current_block);
      i = 0;
      for (; i < lindex->map_size; i++) {
        lookup_index_node lnode = lindex->map_data[i];
        struct tiny_kv_pair *kv_pair =
            (struct tiny_kv_pair *)malloc(sizeof(struct tiny_kv_pair));
        if (tb_read_blob(lnode.blob_id, buffer) < 0) {
          printf("ERROR: recover_unordered_map: Could not read blob\n");
          // handle error
          exit(EXIT_FAILURE);
        }
        if (DEBUG)
          printf("Attempting to recover kv_pair with offsey %lu in blob %u\n",
                 lnode.blob_offset, lnode.blob_id);
        memcpy(&kv_pair->key_size, buffer + lnode.blob_offset,
               sizeof(kv_pair->key_size));
        memcpy(&kv_pair->value_size,
               buffer + lnode.blob_offset + sizeof(kv_pair->key_size),
               sizeof(kv_pair->value_size));

        kv_pair->key = (char *)malloc(kv_pair->key_size);
        memcpy(kv_pair->key,
               buffer + lnode.blob_offset + sizeof(kv_pair->key_size) +
                   sizeof(kv_pair->value_size),
               kv_pair->key_size);
        /*
                kv_pair->value = (char *)malloc(kv_pair->value_size);
                memcpy(kv_pair->value,
                       buffer + lnode.blob_offset + sizeof(kv_pair->key_size) +
                           kv_pair->key_size + sizeof(kv_pair->value_size),
                       kv_pair->value_size);
        */
        lookup_.emplace(std::string(kv_pair->key), lnode);
        free(kv_pair->key);
        //      free(kv_pair->value);
        free(kv_pair);
        kv_pair = NULL;
      }

      printf("Recovered %d kvpairs from blob %u\n", i, current_block);
      current_block = lindex->next_blob_id;
      first_block = 0;
    }
    //  build unordered map with data from this blob
    //  if end of blob: continue to the next blob(if exists) and repeat

    free(buffer);
    buffer = NULL;
  }
}

void tiny_index::persist_unordered_map() {
  int total_kvs = lookup_.size();
  lookup_index *lindex =
      (lookup_index *)memalign(FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);
  lindex->next_blob_id = 0;
  int i = 0;
  bid_t current_blob_id = LOOKUP_TABLE_BLOB_ID;
  for (auto &kv : lookup_) {
    memcpy(&lindex->map_data[i], &kv.second, sizeof(lookup_index_node));
    i++;
    lindex->map_size = i;
    if (i >= MAX_LOOKUP_INDEX_NODE_COUNT) {
      // lets allocate a new blob for the rest of the index_nodes
      bid_t next_blob_id = tb_allocate_blob(HANDLE);
      lindex->next_blob_id = next_blob_id;
      // lindex->map_size = i;
      // we ve written so far "i" kv_pairs
      // total_kvs -= i;
      // write the first index nodes
      if (tb_write_blob(current_blob_id, lindex) < 0) {
        printf("ERROR: could not persist unordered map in blob\n");
        exit(EXIT_FAILURE);
      }
      //  lets allocate a new buffer for the next blob
      lindex =
          (lookup_index *)memalign(FS_LOGICAL_BLK_SIZE, sizeof(lookup_index));
      lindex->next_blob_id = 0;
      // and now write the rest of the keys to the next blob
      current_blob_id = next_blob_id;
      i = 0;
    }
  }
  lindex->map_size = i;
  tb_write_blob(current_blob_id, lindex);
  free(lindex);
  lindex = NULL;
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
