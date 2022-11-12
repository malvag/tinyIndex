#ifndef TINYBLOB_INCLUDED
#define TINYBLOB_INCLUDED

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <bitset>

#include "config.h"

typedef uint64_t __attribute__((__may_alias__)) bid_t;
enum owner { USER, HANDLE};

struct blob {
  bid_t id;
  uint64_t block_id;
  uint32_t size_used;
  enum owner owner;
} __attribute__((__packed__));

struct tiny_blob_handle_t {
  struct blob **table;
  char *location;
  bid_t bidno;
  bid_t table_size;

  off_t log_offset;
  pthread_rwlock_t *lock;
  pthread_rwlockattr_t *lock_attr;
  int file_descriptor;
} __attribute__((__packed__));

typedef struct tiny_blob_handle_t tiny_blob_handle_t;
typedef struct blob blob;

#define RWLOCK_INIT(L, attr) pthread_rwlock_init(L, attr)
#define RWLOCK_WRLOCK(L) pthread_rwlock_wrlock(L)
#define RWLOCK_RDLOCK(L) pthread_rwlock_rdlock(L)
#define RWLOCK_UNLOCK(L) pthread_rwlock_unlock(L)

void tb_flush(
    void); // Flush all data and metadata to the disk.  NOTE! Be careful when
           // you try to persist structs, you need to add
           // __attribute__((__packed__)); otherwise the compiler may add
           // padding for alignment purposes. Read more in the following link
           // https://gcc.gnu.org/onlinedocs/gcc-3.3/gcc/Type-Attributes.html

void tb_init(
    char *location); // Given a directory or a file, it reads the library state
                     // in memory and is able to perform I/O to the store.

void tb_shutdown(
    void); // Clean shutdown of the store; Flush all data and metadata so that
           // it is possible to start from the same files/device.

bid_t tb_allocate_blob(
    enum owner); // On success it returns the tiny_index/id of the allocated blob in
           // the store. On failure it returns -1; When using files you can use
           // a naming scheme “Bxxx” to name each blob file.

void tb_free_blob(bid_t); // Given a valid tiny_index from a successful
                          // allocate_blob call, it frees the blob.

int tb_write_blob(
    bid_t,
    void *data); // Perform a synchronous write (using issue_write() or
                 // otherwise) from the data buffer. Return success or failure.

int tb_read_blob(
    bid_t,
    void *data); // Perform a synchronous read (using issue_read() or otherwise)
                 // to the data buffer. Return success or failure.

char *get_blob_filepath(bid_t blob_id);
// /*(extra credit)*/
// iohandle_t tb_write_blob_issue(bid_t, void *data); // Issue an asynchronous a
// write operation (of the full blob) from the data buffer to tiny_index. Return
// a tiny_blob_handle_t to identify the I/O.

// /*(extra credit)*/
// iohandle_t tb_read_blob_issue(bid_t, void *data) // Issue an asynchronous
// read  (of the full blob)  to tiny_index a in buffer data (data must be a
// valid, preallocated buffer). Return a tiny_blob_handle_t to identify the I/O.

// /*(extra credit)*/
// void tb_wait_io(iohandle_t); // Wait for the asynchronous id to be performed.

bid_t find_next_available_blob(uint32_t size_needed);
#endif
