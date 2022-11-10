#ifndef TINY_CONFIG_INCLUDED
#define TINY_CONFIG_INCLUDED

#define DEVICE_BLOCK_SIZE 4096
#define FS_LOGICAL_BLK_SIZE 512
#define MAX_BLOBS (16LU * 1024LU)
//#define MAX_KEY_SIZE 44

#define FREE 1
#define ALLOCATED 0
#define FILE_BLOB_SIZE (16LU * 1024LU)

// index

#define MAX_LOOKUP_INDEX_NODE_COUNT                                            \
  (FILE_BLOB_SIZE - sizeof(bid_t) - sizeof(uint32_t)) /                        \
      sizeof(struct lookup_index_node)

// allocator
/*
  - db.bin:
  | tiny_blob_handle_t | BITMAP | BLOBS_META | LOOKUP_TABLE | BLOBS + INDEX |
  -

*/
#define HANDLE_OFFSET 0
#define HANDLE_FILE_SIZE 4096

#define BITMAP_OFFSET HANDLE_FILE_SIZE
#define BITMAP_FILE_SIZE MAX_BLOBS

#define BLOBS_META_FILE_SIZE MAX_BLOBS * sizeof(blob)
#define BLOBS_META_OFFSET (BITMAP_OFFSET + BITMAP_FILE_SIZE)

#define DATA_OFFSET (BLOBS_META_OFFSET + BLOBS_META_FILE_SIZE)

#define DB_FILE_SIZE                                                           \
  ((MAX_BLOBS * DEVICE_BLOCK_SIZE) + HANDLE_FILE_SIZE + BITMAP_FILE_SIZE +     \
   BLOBS_META_FILE_SIZE)

#define LOG_BUFFER_SIZE 2048
#define LOG_FILE_SIZE (16LU * 1024LU * 10 * LOG_BUFFER_SIZE) // 320MB

#define DB_FILE_NAME "db.bin"
#define LOG_FILE_NAME "log.bin"

#endif
