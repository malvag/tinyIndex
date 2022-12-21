#pragma once


#define DEVICE_BLOCK_SIZE 4096LU
#define FS_LOGICAL_BLK_SIZE 512LU
#define MAX_BLOBS (16LU * 1024LU) * 4 * 4
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
#define HANDLE_OFFSET 0LU
#define HANDLE_FILE_SIZE 4096LU

#define BITMAP_OFFSET HANDLE_FILE_SIZE
#define BITMAP_FILE_SIZE MAX_BLOBS

#define BLOBS_META_FILE_SIZE MAX_BLOBS * sizeof(blob)
#define BLOBS_META_OFFSET (BITMAP_OFFSET + BITMAP_FILE_SIZE)

#define DATA_OFFSET (BLOBS_META_OFFSET + BLOBS_META_FILE_SIZE)

#define DB_FILE_SIZE                                                           \
  ((MAX_BLOBS * FILE_BLOB_SIZE) + HANDLE_FILE_SIZE + BITMAP_FILE_SIZE +     \
   BLOBS_META_FILE_SIZE)

#define USE_LOG
#define LOG_BUFFER_SIZE 2048LU
#define LOG_FILE_SIZE (1024LU * 2048U * LOG_BUFFER_SIZE) // 4GB

#define LOG_SHIFTED_BUFFER_SIZE (16LU * 1024LU)

#define DB_FILE_NAME "db.bin"
#define LOG_FILE_NAME "log.bin"



// #endif
