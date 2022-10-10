#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tinyblob.h"

#define INITIAL_BLOBS_NUMBER 10

bid_t generate_blob_id(void) { return tb_handle->bidno++; }
void sync_flush_handle_buffer();
void sync_flush_blob_buffer();
void tb_recover(char *location);

size_t round_up_to_blksize(size_t);

// return path of the blob file
char *get_blob_filepath(bid_t blob_id) {
  assert(tb_handle->location != NULL);
  char filepath[4096];
  if (sprintf(filepath, "%s/B%lu", tb_handle->location, blob_id) < 0) {
    ("%s: Could not create path %s for blob with id %lu\n", __func__, filepath,
     blob_id);
    return "";
  }

  return strdup(filepath);
}

char *get_filepath(char *dir, char *filename) {
  char filepath[4096];
  if (sprintf(filepath, "%s/%s", dir, filename) < 0) {
    printf("%s: Could not create path %s with provided filaname %s\n", __func__,
           filepath, filename);
    return "";
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
  char *handle_file = get_filepath(location, "HANDLE");

  // if we dont find a store inside location then create one
  if (access(handle_file, F_OK) != -1) {
    tb_recover(location);
    assert(tb_handle);
    /*printf("Found store with %d %d \n", tb_handle->bidno,
           tb_handle->table_size);*/
  } else {
    posix_memalign((void **)&tb_handle, FS_LOGICAL_BLK_SIZE, sizeof(handle));
    tb_handle->table_size = INITIAL_BLOBS_NUMBER;
    tb_handle->bidno = 0;
    tb_handle->table = (blob **)calloc(tb_handle->table_size, sizeof(blob *));
    tb_handle->location = strdup(location);
  }

  free(handle_file);
}

// On success it returns the index/id of the allocated blob in the store. On
// failure it returns -1; When using files you can use a naming scheme “Bxxx” to
// name each blob file.
bid_t tb_allocate_blob(void) {
  blob *new_blob = NULL;
  posix_memalign((void **)&new_blob, FS_LOGICAL_BLK_SIZE, sizeof(blob));

  new_blob->id = generate_blob_id();
  if (tb_handle->bidno - 1 == tb_handle->table_size) {
    tb_handle->table_size *= 2;
    tb_handle->table =
        realloc(tb_handle->table, tb_handle->table_size * sizeof(blob *));
  }
  tb_handle->table[new_blob->id] = new_blob;
  char *filepath = get_blob_filepath(new_blob->id);

  int fd = open(filepath, O_CREAT | O_RDWR | O_DIRECT, 0600);

  if (fd < 0) {
    printf("%s: Could not open file (path %s) with bid %lu\n", __func__,
           filepath, new_blob->id);
    return -1;
  }

  free(filepath);
  if (close(fd)) {
    printf("%s: Could not close file with bid %lu\n", __func__, new_blob->id);
    return -1;
  }

  return new_blob->id;
}

// Perform a synchronous write (using issue_write() or otherwise) from the data
// buffer. Return success or failure.
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
  char *filepath = get_blob_filepath(b->id);

  int fd = open(filepath, O_WRONLY | O_DIRECT, 0600);
  if (fd < 0) {
    printf("%s: %s :Could not open file (path %s) with bid %lu\n",
           strerror(errno), __func__, filepath, b->id);
    return -1;
  }

  // align the data given from the user
  char *data_aligned = NULL;
  posix_memalign((void **)&data_aligned, FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);
  memcpy(data_aligned, data, FILE_BLOB_SIZE);

  int ret = write(fd, data_aligned, FILE_BLOB_SIZE);
  if (ret < 0) {
    printf("%s: %s: Could not write file (path %s) with bid %lu\n",
           strerror(errno), __func__, filepath, b->id);
    return -1;
  }

  free(filepath);
  close(fd);
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
  char *filepath = get_blob_filepath(b->id);

  int fd = open(filepath, O_RDONLY | O_DIRECT);

  if (fd < 0) {
    printf("%s: Could not open file (path %s) with bid %lu\n", __func__,
           filepath, b->id);

    return -1;
  }

  // align the data given from the user
  char *buffer_aligned = NULL;
  posix_memalign((void **)&buffer_aligned, FS_LOGICAL_BLK_SIZE, FILE_BLOB_SIZE);

  int ret = read(fd, buffer_aligned, FILE_BLOB_SIZE);
  memcpy(data, buffer_aligned, FILE_BLOB_SIZE);
  if (ret < 0) {
    printf("%s: Could not read file (path %s) with bid %lu\n", __func__,
           filepath, b->id);
    return -1;
  }
  free(filepath);
  close(fd);
}

// Given a valid index from a successful allocate_blob call, it frees the blob.
void tb_free_blob(bid_t blob_id) {
  if (blob_id >= tb_handle->bidno) {
    printf("Incorrect blob_id, is this valid?\n");
    return;
  }

  blob *b = tb_handle->table[blob_id];
  char *filepath = get_blob_filepath(blob_id);

  remove(filepath); // calls unlink(2)
  free(b);
  free(filepath);
  tb_handle->table[blob_id] = NULL; // TODO: what about fragmentation(?)
}

int sync_flush_buffer(char *data, uint64_t buffer_size, char *filename) {
  int fd = open(filename, O_WRONLY | O_SYNC | O_DIRECT | O_CREAT, 0600);
  if (fd < 0) {
    printf("%s: %s: Could not open file (path %s)\n", strerror(errno), __func__,
           filename);

    return -1;
  }

  int ret = write(fd, data, buffer_size);
  if (ret < 0) {
    printf("%s: %s: Could not write file (path %s)\n", strerror(errno),
           __func__, filename);
    return -1;
  }
  close(fd);
  return 0;
}

// Flush all data and metadata to the disk.
void tb_flush(void) {
  // create blobs_file: contains blob's metadata
  sync_flush_blob_buffer();
  // create handle_file: contains handle's struct
  sync_flush_handle_buffer();
}

// Clean shutdown of the store; Flush all data and metadata so that it is
// possible to start from the same files/device.
void tb_shutdown(void) {
  tb_flush();
  // all data and metadata should be flushed to disk
  // free everything
  free(tb_handle->location);
  for (uint32_t i = 0; i < tb_handle->bidno; ++i) {
    free(tb_handle->table[i]);
  }
  free(tb_handle->table);
  free(tb_handle);
}

void sync_flush_blob_buffer() {
  size_t blobs_file_size = round_up_to_blksize(tb_handle->bidno * sizeof(blob));

  char *aligned_blobs_buffer = NULL;
  posix_memalign((void **)&aligned_blobs_buffer, FS_LOGICAL_BLK_SIZE,
                 blobs_file_size);

  for (uint32_t i = 0; i < tb_handle->bidno; i++) {
    memcpy(&aligned_blobs_buffer[i * sizeof(blob)], tb_handle->table[i],
           sizeof(blob));
    // printf("INFO: flushed blob %d\n",tb_handle->table[i]->id);
  }

  // TODO: check for errors
  char *filepath = get_filepath(tb_handle->location, "BLOBS");
  if (sync_flush_buffer(aligned_blobs_buffer, blobs_file_size, filepath) < 0) {
    printf("Could not persist buffer for blobs\n");
  }

  free(filepath);
  free(aligned_blobs_buffer);
}

void sync_flush_handle_buffer() {
  size_t handle_file_size = round_up_to_blksize(sizeof(handle));

  char *aligned_handle_buffer = NULL;
  posix_memalign((void **)&aligned_handle_buffer, FS_LOGICAL_BLK_SIZE,
                 handle_file_size);

  memcpy(aligned_handle_buffer, tb_handle, handle_file_size);

  // TODO: check for errors
  char *filepath = get_filepath(tb_handle->location, "HANDLE");
  if (sync_flush_buffer(aligned_handle_buffer, handle_file_size, filepath) <
      0) {
    printf("Could not persist buffer for handle\n");
  }
  free(filepath);
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

int sync_recover_buffer(char **data, uint64_t buffer_size, char *filename) {
  int fd = open(filename, O_RDONLY | O_SYNC | O_DIRECT, 0600);
  if (fd < 0) {
    printf("%s: %s: Could not open file (path %s)\n", strerror(errno), __func__,
           filename);

    return -1;
  }

  int ret = read(fd, *data, buffer_size);
  if (ret < 0) {
    printf("%s: %s: Could not read file (path %s)\n", strerror(errno), __func__,
           filename);
    return -1;
  }
  close(fd);
  return 0;
}

void sync_recover_handle(char *location) {
  size_t handle_file_size = round_up_to_blksize(sizeof(handle));

  char *aligned_handle_buffer = NULL;
  posix_memalign((void **)&aligned_handle_buffer, FS_LOGICAL_BLK_SIZE,
                 handle_file_size);

  // TODO: check for errors
  char *filepath = get_filepath(location, "HANDLE");
  if (sync_recover_buffer(&aligned_handle_buffer, handle_file_size, filepath) <
      0) {
    printf("Could not recover buffer for handle\n");
    return;
  }
  free(filepath);
  tb_handle = (void *)aligned_handle_buffer;
}

void sync_recover_blobs(char *location) {
  size_t blob_file_size = round_up_to_blksize(tb_handle->bidno * sizeof(blob));

  char *aligned_blob_buffer = NULL;
  posix_memalign((void **)&aligned_blob_buffer, FS_LOGICAL_BLK_SIZE,
                 blob_file_size);

  // TODO: check for errors
  char *filepath = get_filepath(location, "BLOBS");
  if (sync_recover_buffer(&aligned_blob_buffer, blob_file_size, filepath) < 0) {
    printf("Could not recover buffer for blob\n");
    return;
  }

  for (size_t i = 0; i < tb_handle->bidno; i++) {
    blob *new_blob = NULL;
    posix_memalign((void **)&new_blob, FS_LOGICAL_BLK_SIZE, sizeof(blob));
    memcpy(new_blob, &aligned_blob_buffer[i * sizeof(blob)], sizeof(blob));
    tb_handle->table[i] = new_blob;
    /*printf("INFO: recovered BLOB %d\n",new_blob->id);*/
  }

  free(filepath);
  free(aligned_blob_buffer);
}

void tb_recover(char *location) {
  if (location == NULL) {
    printf("ERROR: could not recover, due to the location that was given is "
           "NULL\n");
    return;
  }

  sync_recover_handle(location);
  tb_handle->table = (blob **)calloc(tb_handle->table_size, sizeof(blob *));
  tb_handle->location = strdup(location);

  sync_recover_blobs(location);
}
