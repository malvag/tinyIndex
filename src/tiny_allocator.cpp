#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOCK_TINYBLOB

#include "tiny_allocator.h"

struct tiny_blob_handle_t *tb_handle = NULL;

std::bitset<MAX_BLOBS> bitmap;

bid_t generate_blob_id(void) { return tb_handle->bidno++; }
void sync_flush_handle_buffer();
void sync_flush_bitmap_buffer();
void tb_recover(char *location);
void init_block_device(char *filepath);
size_t round_up_to_blksize(size_t);

int init_rwlock() {
  pthread_rwlock_t *rwlock =
      (pthread_rwlock_t *)malloc(sizeof(pthread_rwlock_t));
  pthread_rwlockattr_t *rwlock_attr =
      (pthread_rwlockattr_t *)malloc(sizeof(pthread_rwlock_t));
  pthread_rwlockattr_init(rwlock_attr);
  pthread_rwlockattr_setpshared(rwlock_attr, PTHREAD_PROCESS_SHARED);
  if (pthread_rwlock_init(rwlock, rwlock_attr) == 0) {
    tb_handle->lock = rwlock;
    tb_handle->lock_attr = rwlock_attr;
    return 0;
  }
  return -1;
}

void destroy_rwlock() {
  pthread_rwlockattr_destroy(tb_handle->lock_attr);
  free(tb_handle->lock_attr);
  pthread_rwlock_destroy(tb_handle->lock);
  free(tb_handle->lock);
}

int find_next_free_block() {
  if (bitmap.count() == 0)
    return -1;

  for (int i = 0; i < bitmap.size(); i++) {
    if (bitmap[i] == true) {
      bitmap[i] = false;
      return i;
    }
  }
  return -1;
}

// return path of the blob file
char *get_blob_filepath(bid_t blob_id) {
  assert(tb_handle->location != NULL);
  char filepath[4096] = {0};
  if (sprintf(filepath, "%s/B%lu", tb_handle->location, blob_id) < 0) {
    ("%s: Could not create path %s for blob with id %lu\n", __func__, filepath,
     blob_id);
    return NULL;
  }

  return strdup(filepath);
}

char *get_filepath(std::string dir, std::string filename) {
  char filepath[4096];
  if (sprintf(filepath, "%s/%s", dir.c_str(), filename.c_str()) < 0) {
    printf("%s: Could not create path %s with provided filaname %s\n", __func__,
           filepath, filename);
    return NULL;
  }
  return strdup(filepath);
}

// Given a directory or a file, it reads the library state in memory and is
// able to perform I/O to the store.
void tb_init(char *location) {
  if (location == NULL) {
    printf("ERROR: Should have a location to run on!\n");
    return;
  }
  char *handle_file = get_filepath(location, "db.bin");

  // if we dont find a store inside location then create one
  if (access(handle_file, F_OK) != -1) {
    tb_recover(handle_file);
    assert(tb_handle != NULL);
    /*printf("Found store with %d %d \n", tb_handle->bidno,
           tb_handle->table_size);*/
  } else {
    posix_memalign((void **)&tb_handle, FS_LOGICAL_BLK_SIZE,
                   round_up_to_blksize(sizeof(tiny_blob_handle_t)));
    tb_handle->table_size = MAX_BLOBS;
    tb_handle->bidno = 0;
    tb_handle->table = (blob **)calloc(tb_handle->table_size, sizeof(blob *));
    tb_handle->location = strdup(location);
    init_block_device(handle_file);
  }

  free(handle_file);
}

void init_block_device(char *filepath) {
  assert(tb_handle != NULL);
  assert(tb_handle->location != NULL);
  // open tiny_blob_handle_t
  int fd = open(filepath, O_CREAT | O_RDWR | O_DSYNC | O_DIRECT, 0600);
  if (fd < 0) {
    printf("%s: Could not open block_device file (path: %s)\n", strerror(errno),
           filepath);
  }
  tb_handle->file_descriptor = fd;
  printf("INFO: Created file %s\n", filepath);

  // fallocate
  int ret = posix_fallocate(fd, 0, DB_FILE_SIZE);
  if (ret < 0) {
    printf("%s: Could not fallocate block_file (path: %s)\n", strerror(errno),
           filepath);
    close(fd);
    return;
  }
  printf("INFO: fallocated the file\n");

  // init bitmap
  for (int i = 0; i < MAX_BLOBS; i++)
    bitmap.set(i, true);

  // init lock
  if (init_rwlock() < 0) {
    printf("ERROR: could not initialize rwlock");
  }
}

// On success it returns the tiny_index/id of the allocated blob in the store.
// On failure it returns -1; When using files you can use a naming scheme “Bxxx”
// to name each blob file.
bid_t tb_allocate_blob(enum owner owner) {
  blob *new_blob = NULL;
  posix_memalign((void **)&new_blob, FS_LOGICAL_BLK_SIZE, sizeof(blob));
  RWLOCK_WRLOCK(tb_handle->lock);
  new_blob->id = generate_blob_id();
  new_blob->block_id = find_next_free_block();
  if (new_blob->block_id == -1) {
    printf("ERROR: could not find free block\n");
    return 0;
  }
  new_blob->owner = owner;
  new_blob->size_used = FILE_BLOB_SIZE;
  printf("Allocated new blob %lu %d\n", new_blob->id,new_blob->owner);
  // printf("INFO: Next available block %d\n", new_blob->block_id);
  tb_handle->table[new_blob->id] = new_blob;
  RWLOCK_UNLOCK(tb_handle->lock);
  return new_blob->id;
}

// Perform a synchronous write (using issue_write() or otherwise) from the
// data buffer. Return success or failure.
int tb_write_blob(bid_t blob_id, void *data) {
  if (blob_id >= tb_handle->bidno) {
    printf("Incorrect blob_id %d, is this valid?\n", blob_id);
    return -1;
  }
  if (!data) {
    printf("Cannot write null data into a blob(?), is this valid?\n");
    return -1;
  }
  RWLOCK_WRLOCK(tb_handle->lock);
  blob *b = tb_handle->table[blob_id];

  // align the data given from the user
  char *data_aligned = NULL;
  data_aligned = (char *)memalign(FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);
  memcpy(data_aligned, data, FILE_BLOB_SIZE);

  int ret = pwrite(tb_handle->file_descriptor, data_aligned, FILE_BLOB_SIZE,
                   DATA_OFFSET + b->block_id * FILE_BLOB_SIZE);
  RWLOCK_UNLOCK(tb_handle->lock);
  free(data_aligned);
  if (ret < 0) {
    printf("%s: %s: Could not write blob with bid %lu\n", strerror(errno),
           __func__, b->id);
    return -1;
  }
  return 0;
}

// Perform a synchronous read (using issue_read() or otherwise) to the data
// buffer. Return success or failure.
int tb_read_blob(bid_t blob_id, void *data) {
  if (blob_id >= tb_handle->bidno) {
    printf("Incorrect blob_id %d, is this valid?\n", blob_id);
    return -1;
  }
  if (!data) {
    printf("Not allocated buffer to store\n");
    return -1;
  }
  blob *b = tb_handle->table[blob_id];
  // printf("id:%d block:%lu\n", b->id, b->block_id);

  // align the data given from the user
  char *buffer_aligned = NULL;

  RWLOCK_RDLOCK(tb_handle->lock);
  posix_memalign((void **)&buffer_aligned, FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);
  int ret = pread(tb_handle->file_descriptor, buffer_aligned, FILE_BLOB_SIZE,
                  DATA_OFFSET + b->block_id * FILE_BLOB_SIZE);

  if (ret < 0) {
    printf("%s: Could not read blob with bid %lu\n", __func__, b->id);
    return -1;
  }

  memcpy(data, buffer_aligned, FILE_BLOB_SIZE);
  free(buffer_aligned);
  RWLOCK_UNLOCK(tb_handle->lock);
  return 0;
}

// Given a valid tiny_index from a successful allocate_blob call, it frees the
// blob.
void tb_free_blob(bid_t blob_id) {
  RWLOCK_WRLOCK(tb_handle->lock);
  if (blob_id >= tb_handle->bidno) {
    printf("Incorrect blob_id, is this valid?\n");
    return;
  }

  blob *b = tb_handle->table[blob_id];
  bitmap.set(b->block_id, true); // free the space

  free(b);
  tb_handle->table[blob_id] = NULL; // TODO: what about
  RWLOCK_UNLOCK(tb_handle->lock);
}

// Clean shutdown of the store; Flush all data and metadata so that it is
// possible to start from the same files/device.
void tb_shutdown(void) {
  RWLOCK_WRLOCK(tb_handle->lock);
  // all data and metadata should be flushed to disk
  tb_flush();
  // free everything
  close(tb_handle->file_descriptor);
  RWLOCK_UNLOCK(tb_handle->lock);
  destroy_rwlock();

  for (uint32_t i = 0; i < tb_handle->bidno; ++i) {
    free(tb_handle->table[i]);
  }
  free(tb_handle->location);
  free(tb_handle->table);
  free(tb_handle);
}

void sync_flush_bitmap_buffer() {
  size_t bitmap_file_size = BITMAP_FILE_SIZE;

  uint8_t *aligned_bitmap_buffer = NULL;
  posix_memalign((void **)&aligned_bitmap_buffer, FS_LOGICAL_BLK_SIZE,
                 bitmap_file_size);

  for (uint32_t i = 0; i < bitmap.size(); i++) {
    aligned_bitmap_buffer[i] = bitmap[i];
  }

  int ret = pwrite(tb_handle->file_descriptor, aligned_bitmap_buffer,
                   bitmap_file_size, BITMAP_OFFSET);
  if (ret < 0) {
    printf("%s: %s: Could not write bitmap buffer\n", strerror(errno),
           __func__);
  }

  free(aligned_bitmap_buffer);
}

void sync_flush_blobs_buffer() {
  size_t blobs_file_size = BLOBS_META_FILE_SIZE;

  char *aligned_blobs_buffer = NULL;
  posix_memalign((void **)&aligned_blobs_buffer, FS_LOGICAL_BLK_SIZE,
                 blobs_file_size);

  for (uint32_t i = 0; i < tb_handle->bidno; i++) {
    memcpy(&aligned_blobs_buffer[i * sizeof(blob)], tb_handle->table[i],
           sizeof(blob));
    // printf("INFO: flushed blob %d\n",tb_handle->table[i]->id);
  }

  int ret = pwrite(tb_handle->file_descriptor, aligned_blobs_buffer,
                   blobs_file_size, BLOBS_META_OFFSET);
  if (ret < 0) {
    printf("%s: %s: Could not write blob_metadata buffer\n", strerror(errno),
           __func__);
  }

  free(aligned_blobs_buffer);
}

void sync_flush_handle_buffer() {
  size_t handle_file_size = round_up_to_blksize(sizeof(tiny_blob_handle_t));

  char *aligned_handle_buffer = NULL;
  posix_memalign((void **)&aligned_handle_buffer, FS_LOGICAL_BLK_SIZE,
                 handle_file_size);

  memcpy(aligned_handle_buffer, tb_handle, handle_file_size);

  int ret = pwrite(tb_handle->file_descriptor, aligned_handle_buffer,
                   handle_file_size, HANDLE_OFFSET);

  if (ret < 0) {
    printf("%s: %s: Could not write tiny_blob_handle_t buffer \n",
           strerror(errno), __func__);
  }
  free(aligned_handle_buffer);
}

// find appropriate multiple of 512 for buffer_size
size_t round_up_to_blksize(size_t buffer_size) {
  if (buffer_size % FS_LOGICAL_BLK_SIZE > 0) {
    buffer_size =
        FS_LOGICAL_BLK_SIZE * ((buffer_size / FS_LOGICAL_BLK_SIZE) + 1);
  } else {
    buffer_size = FS_LOGICAL_BLK_SIZE * (buffer_size / FS_LOGICAL_BLK_SIZE);
  }
  return buffer_size;
}

// Flush all data and metadata to the disk.
void tb_flush(void) {
  // create blobs_file
  sync_flush_bitmap_buffer();
  // flush tiny_blob_handle_t buffer
  sync_flush_handle_buffer();
  // flush blobs metadata
  sync_flush_blobs_buffer();
}

void sync_recover_handle(int fd) {
  size_t handle_file_size = round_up_to_blksize(sizeof(tiny_blob_handle_t));

  char *aligned_handle_buffer = NULL;
  posix_memalign((void **)&aligned_handle_buffer, FS_LOGICAL_BLK_SIZE,
                 handle_file_size);

  int ret = pread(fd, aligned_handle_buffer, handle_file_size, HANDLE_OFFSET);
  if (ret < 0) {
    printf("%s: %s: Could not read file \n", strerror(errno), __func__);
    exit(-1);
  }
  tb_handle = (tiny_blob_handle_t *)aligned_handle_buffer;
}

void sync_recover_blobs() {
  size_t blob_file_size = BLOBS_META_FILE_SIZE;

  char *aligned_blob_buffer = NULL;
  posix_memalign((void **)&aligned_blob_buffer, FS_LOGICAL_BLK_SIZE,
                 blob_file_size);

  int ret = pread(tb_handle->file_descriptor, aligned_blob_buffer,
                  blob_file_size, BLOBS_META_OFFSET);
  if (ret < 0) {
    printf("%s: %s: Could not read file \n", strerror(errno), __func__);
    exit(-1);
  }

  int recovered_blob_count = 0;
  for (size_t i = 0; i < tb_handle->bidno; i++) {
    blob *new_blob = (blob *)memalign(FS_LOGICAL_BLK_SIZE, sizeof(blob));
    memcpy(new_blob, &aligned_blob_buffer[i * sizeof(blob)], sizeof(blob));
    tb_handle->table[i] = new_blob;
    recovered_blob_count++;
  }

  printf("INFO: recovered blobs: %d/%d\n", recovered_blob_count,
         tb_handle->bidno);
  free(aligned_blob_buffer);
}

void sync_recover_bitmap() {
  size_t bitmap_file_size = BITMAP_FILE_SIZE;

  uint8_t *aligned_bitmap_buffer = NULL;
  posix_memalign((void **)&aligned_bitmap_buffer, FS_LOGICAL_BLK_SIZE,
                 bitmap_file_size);

  int ret = pread(tb_handle->file_descriptor, aligned_bitmap_buffer,
                  bitmap_file_size, BITMAP_OFFSET);
  if (ret < 0) {
    printf("%s: %s: Could not read file \n", strerror(errno), __func__);
    exit(-1);
  }
  for (uint32_t i = 0; i < bitmap.size(); i++) {
    bitmap[i] = aligned_bitmap_buffer[i];
  }

  free(aligned_bitmap_buffer);
}

void tb_recover(char *location) {
  if (location == NULL) {
    printf("ERROR: could not recover, due to the location that was given is "
           "NULL\n");
    return;
  }

  // open tiny_blob_handle_t
  int fd = open(location, O_RDWR | O_DSYNC | O_DIRECT, 0600);
  if (fd < 0) {
    printf("%s: Could not open block_device file (path: %s)\n", strerror(errno),
           location);
  }

  printf("INFO: Opened db.bin file %s\n", location);

  sync_recover_handle(fd);
  tb_handle->file_descriptor = fd;
  tb_handle->table = (blob **)calloc(tb_handle->table_size, sizeof(blob *));
  tb_handle->location = strdup(location);

  sync_recover_bitmap();

  sync_recover_blobs();

  // init lock
  if (init_rwlock() < 0) {
    printf("ERROR: could not initialize rwlock");
  }
}
bid_t find_next_available_blob(uint32_t size_needed) {
  RWLOCK_RDLOCK(tb_handle->lock);
  for (int i = 0; i < tb_handle->bidno; i++) {
    blob *b = tb_handle->table[i];
    if (b->owner == USER && b->size_used > size_needed) {
      RWLOCK_UNLOCK(tb_handle->lock);
      return i;
    }
  }
  RWLOCK_UNLOCK(tb_handle->lock);
  bid_t bid = tb_allocate_blob(USER);
  return bid;
}
