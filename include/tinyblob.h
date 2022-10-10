#ifndef TINYBLOB_INCLUDED
#define TINYBLOB_INCLUDED

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#ifdef BLOCK_TINYBLOB
#include <bitset>
#endif

#define DEVICE_BLOCK_SIZE 4096
#define FS_LOGICAL_BLK_SIZE 512
#define FREE 1
#define ALLOCATED 0


#define FILE_BLOB_SIZE DEVICE_BLOCK_SIZE

typedef uint64_t bid_t;
static struct handle *tb_handle;

struct blob{
    bid_t id;
#ifdef BLOCK_TINYBLOB
    uint64_t block_id;
#endif
} __attribute__((__packed__));


#define MAX_BLOBS 2048
#define DEVICE_BLOCK_FILE_SIZE (MAX_BLOBS * DEVICE_BLOCK_SIZE)

struct handle{
    struct blob** table;
    char* location;
    bid_t bidno;
    bid_t table_size;

    pthread_mutex_t* lock;
    
#ifdef BLOCK_TINYBLOB
    int file_descriptor;
#endif
}  __attribute__((__packed__));


/*
           | ---- ----- superblock  ----- --- |-------- data -------------- -- - -
  db.bin:  |  HANDLE  |  BITMAP  | BLOBS_META |  BLOB_VALUE0 - BLOB_VALUE1 - - -

*/

#define BITMAP_FILE_SIZE MAX_BLOBS * sizeof(uint8_t)
#define BLOBS_META_FILE_SIZE 4096
#define BITMAP_OFFSET 4096 // in reality sizeof(handle)
#define HANDLE_OFFSET 0
#define BLOBS_META_OFFSET (BITMAP_OFFSET + BITMAP_FILE_SIZE)
#define DATA_OFFSET (BLOBS_META_OFFSET + BLOBS_META_FILE_SIZE)
// 
typedef struct handle handle;
typedef struct blob blob;

void tb_flush(void); // Flush all data and metadata to the disk.  NOTE! Be careful when you try to persist structs, you need to add __attribute__((__packed__)); otherwise the compiler may add padding for alignment purposes. Read more in the following link https://gcc.gnu.org/onlinedocs/gcc-3.3/gcc/Type-Attributes.html

void tb_init(char *location); // Given a directory or a file, it reads the library state in memory and is able to perform I/O to the store. 

void tb_shutdown(void); // Clean shutdown of the store; Flush all data and metadata so that it is possible to start from the same files/device.

bid_t tb_allocate_blob(void); // On success it returns the index/id of the allocated blob in the store. On failure it returns -1; When using files you can use a naming scheme “Bxxx” to name each blob file.

void tb_free_blob(bid_t); // Given a valid index from a successful allocate_blob call, it frees the blob.

int tb_write_blob(bid_t, void *data); // Perform a synchronous write (using issue_write() or otherwise) from the data buffer. Return success or failure.

int tb_read_blob(bid_t, void *data); // Perform a synchronous read (using issue_read() or otherwise) to the data buffer. Return success or failure.

char* get_blob_filepath(bid_t blob_id);
// /*(extra credit)*/ 
// iohandle_t tb_write_blob_issue(bid_t, void *data); // Issue an asynchronous a write operation (of the full blob) from the data buffer to blob_index. Return a handle to identify the I/O.

// /*(extra credit)*/ 
// iohandle_t tb_read_blob_issue(bid_t, void *data) // Issue an asynchronous read  (of the full blob)  to blob_index a in buffer data (data must be a valid, preallocated buffer). Return a handle to identify the I/O.

// /*(extra credit)*/ 
// void tb_wait_io(iohandle_t); // Wait for the asynchronous id to be performed.
#endif