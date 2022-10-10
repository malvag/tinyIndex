#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOCK_TINYBLOB

#include "../include/tinyblob.h"

#define INITIAL_BLOBS_NUMBER 10

/*
           | ---- ----- superblock  ----- --- |-------- data -------------- -- - -
  db.bin:  |  HANDLE  |  BITMAP  | BLOBS_META | BLOB_VALUE0 | BLOB_VALUE0 - BLOB_VALUE1 - - -

*/

std::bitset<MAX_BLOBS> bitmap;

bid_t generate_blob_id(void) { return tb_handle->bidno++; }
void sync_flush_handle_buffer();
void sync_flush_bitmap_buffer();
void tb_recover(char *location);
void init_block_device(char *filepath);
size_t round_up_to_blksize(size_t);

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
  char filepath[4096];
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
    posix_memalign((void **)&tb_handle, FS_LOGICAL_BLK_SIZE, sizeof(handle));
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
  // open handle
  int fd = open(filepath, O_CREAT | O_RDWR | O_DSYNC | O_DIRECT, 0600);
  if (fd < 0) {
    printf("%s: Could not open block_device file (path: %s)\n", strerror(errno),
           filepath);
  }
  tb_handle->file_descriptor = fd;
  printf("INFO: Created file %s\n", filepath);

  // fallocate
  int ret = posix_fallocate(fd, 0, DEVICE_BLOCK_FILE_SIZE);
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
  tb_handle->lock = (pthread_mutex_t *)malloc(sizeof(*(tb_handle->lock)));
  if (tb_handle->lock == NULL) {
    exit(EXIT_FAILURE); // TODO: must handle error properly
  }
  if (pthread_mutex_init(tb_handle->lock, NULL) != 0) {
    printf("\n ERROR: mutex init has failed\n");
    close(fd);
    return;
  }
}

// On success it returns the index/id of the allocated blob in the store. On
// failure it returns -1; When using files you can use a naming scheme “Bxxx”
// to name each blob file.
bid_t tb_allocate_blob(void) {
  blob *new_blob = NULL;
  posix_memalign((void **)&new_blob, FS_LOGICAL_BLK_SIZE, sizeof(blob));

  new_blob->id = generate_blob_id();
  printf("new id %d\n", new_blob->id);
  // exit(1);
  if (tb_handle->bidno - 1 == tb_handle->table_size) {
    tb_handle->table_size *= 2;
    tb_handle->table = (blob **)realloc(tb_handle->table,
                                        tb_handle->table_size * sizeof(blob *));
  }

  new_blob->block_id = find_next_free_block();
  if (new_blob->block_id == -1) {
    printf("ERROR: could not find free block\n");
    return -1;
  }
  // printf("INFO: Next available block %d\n", new_blob->block_id);
  tb_handle->table[new_blob->id] = new_blob;

  return new_blob->id;
}

// Perform a synchronous write (using issue_write() or otherwise) from the
// data buffer. Return success or failure.
int tb_write_blob(bid_t blob_id, void *data) {
  if (blob_id >= tb_handle->bidno) {
    printf("Incorrect blob_id, is this valid?\n");
    return -1;
  }
  if (!data) {
    printf("Cannot write null data into a blob(?), is this valid?\n");
    return -1;
  }
  blob *b = tb_handle->table[blob_id];

  // align the data given from the user
  char *data_aligned = NULL;
  data_aligned = (char *)memalign(FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);
  memcpy(data_aligned, data, FILE_BLOB_SIZE);

  int ret = pwrite(tb_handle->file_descriptor, data_aligned, FILE_BLOB_SIZE,
                   DATA_OFFSET + b->block_id * FILE_BLOB_SIZE);
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
    printf("Incorrect blob_id, is this valid?\n");
    return -1;
  }
  if (!data) {
    printf("Not allocated buffer to store\n");
    return -1;
  }
  blob *b = tb_handle->table[blob_id];

  // align the data given from the user
  char *buffer_aligned = NULL;
  posix_memalign((void **)&buffer_aligned, FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);

  int ret = pread(tb_handle->file_descriptor, buffer_aligned, FILE_BLOB_SIZE,
                  DATA_OFFSET + b->block_id * FILE_BLOB_SIZE);
  memcpy(data, buffer_aligned, FILE_BLOB_SIZE);

  if (ret < 0) {
    printf("%s: Could not read blob with bid %lu\n", __func__, b->id);
    return -1;
  }
  return 0;
}

// Given a valid index from a successful allocate_blob call, it frees the
// blob.
void tb_free_blob(bid_t blob_id) {
  if (blob_id >= tb_handle->bidno) {
    printf("Incorrect blob_id, is this valid?\n");
    return;
  }

  blob *b = tb_handle->table[blob_id];
  bitmap.set(b->block_id, true); // free the space

  free(b);
  tb_handle->table[blob_id] = NULL; // TODO: what about fragmentation(?)
}

// Flush all data and metadata to the disk.
void tb_flush(void) {
  // create blobs_file: contains blob's metadata
  sync_flush_bitmap_buffer();
  // create handle_file: contains handle's struct
  sync_flush_handle_buffer();
}

// Clean shutdown of the store; Flush all data and metadata so that it is
// possible to start from the same files/device.
void tb_shutdown(void) {
  // all data and metadata should be flushed to disk
  tb_flush();
  // free everything
  close(tb_handle->file_descriptor);
  free(tb_handle->location);
  for (uint32_t i = 0; i < tb_handle->bidno; ++i) {
    free(tb_handle->table[i]);
  }
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
  size_t handle_file_size = round_up_to_blksize(sizeof(handle));

  char *aligned_handle_buffer = NULL;
  posix_memalign((void **)&aligned_handle_buffer, FS_LOGICAL_BLK_SIZE,
                 handle_file_size);

  memcpy(aligned_handle_buffer, tb_handle, handle_file_size);

  int ret = pwrite(tb_handle->file_descriptor, aligned_handle_buffer,
                   handle_file_size, HANDLE_OFFSET);

  if (ret < 0) {
    printf("%s: %s: Could not write handle buffer \n", strerror(errno),
           __func__);
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

void sync_recover_handle(int fd) {
  size_t handle_file_size = round_up_to_blksize(sizeof(handle));

  char *aligned_handle_buffer = NULL;
  posix_memalign((void **)&aligned_handle_buffer, FS_LOGICAL_BLK_SIZE,
                 handle_file_size);

  int ret = pread(fd, aligned_handle_buffer,
                  handle_file_size, HANDLE_OFFSET);
  if (ret < 0) {
    printf("%s: %s: Could not read file \n", strerror(errno), __func__);
    exit(-1);
  }
  tb_handle = (handle *)aligned_handle_buffer;
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

  for (size_t i = 0; i < tb_handle->bidno; i++) {
    blob *new_blob = NULL;
    posix_memalign((void **)&new_blob, FS_LOGICAL_BLK_SIZE, sizeof(blob));
    memcpy(new_blob, &aligned_blob_buffer[i * sizeof(blob)], sizeof(blob));
    tb_handle->table[i] = new_blob;
    printf("INFO: recovered BLOB %d\n",new_blob->id);
  }

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

  // open handle
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
  tb_handle->lock = (pthread_mutex_t *)malloc(sizeof(*(tb_handle->lock)));
  if (tb_handle->lock == NULL) {
    exit(EXIT_FAILURE); // TODO: must handle error properly
  }
  if (pthread_mutex_init(tb_handle->lock, NULL) != 0) {
    printf("\n ERROR: mutex init has failed\n");
    close(fd);
    return;
  }
}
