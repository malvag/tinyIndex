#include "tiny_log.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>

#include "tiny_allocator.h"
#include "utilities.h"
void log_handle_t::allocate_new_log_write_buffer() {
  if (write_buffer_ != NULL)
    free(write_buffer_);
  write_buffer_ = (char *)calloc(1, LOG_BUFFER_SIZE);
  write_buffer_size_ = LOG_BUFFER_SIZE;
  write_buffer_offset_ = 0;
}

void log_handle_t::allocate_new_log_read_buffer() {
  if (read_buffer_ != NULL)
    free(read_buffer_);
  read_buffer_ = (char *)memalign(FS_LOGICAL_BLK_SIZE, LOG_BUFFER_SIZE);
  read_buffer_size_ = LOG_BUFFER_SIZE;
  read_buffer_offset_ = 0;
}

void log_handle_t::append(tiny_kv_pair *kv) {
  // since we have O_APPEND we get the offset
  ssize_t amount_written, position;

  /* Write out the data.  */
  lseek(fd_, write_log_offset_, SEEK_SET);
  if (try_write_to_log_buffer(kv) < 0) {
    // if we dont have any free space inside the buffer
    // put the current kv pair into the new buffer
    // then flush the buffer to the log and
    char *buffer_to_be_written;
    posix_memalign((void **)&buffer_to_be_written, FS_LOGICAL_BLK_SIZE,
                   LOG_BUFFER_SIZE);
    memcpy(buffer_to_be_written, write_buffer_, LOG_BUFFER_SIZE);
    amount_written = write(fd_, buffer_to_be_written, write_buffer_size_);
    free(buffer_to_be_written);
    if (amount_written != write_buffer_size_) {
      perror("Could not write buffer in log");
      return;
    }
    write_log_offset_ = lseek(fd_, 0, SEEK_CUR);
    if (write_log_offset_ == -1) {
      perror("Could not get cursor in log");
      return;
    }
    //    printf("Persisted kvs into log\n", kv->key, kv->value);
    allocate_new_log_write_buffer();
    if (try_write_to_log_buffer(kv) == -1)
      return;
  } else {
    printf("Stored kv[%s:%s] into buffer, offset %zu\n", kv->key, kv->value,
           write_buffer_offset_);
  }

  //   return -1;
}

// Writes kv to write_buffer_ at write_buffer_offset_
// and increments the write_buffer_offset_ with the size of kv
int log_handle_t::try_write_to_log_buffer(tiny_kv_pair *kv) {
  uint32_t true_kvsize = sizeof(kv->key_size) + kv->key_size +
                         sizeof(kv->value_size) + kv->value_size;
  if (write_buffer_size_ <= true_kvsize + write_buffer_offset_)
    return -1;
  // construct tiny_kv_pair buffer
  memcpy(write_buffer_ + write_buffer_offset_, &kv->key_size,
         sizeof(kv->key_size));

  memcpy(write_buffer_ + write_buffer_offset_ + sizeof(kv->key_size),
         &kv->value_size, sizeof(kv->value_size));

  memcpy(write_buffer_ + write_buffer_offset_ + sizeof(kv->key_size) +
             sizeof(kv->value_size),
         kv->key, kv->key_size);

  memcpy(write_buffer_ + write_buffer_offset_ + sizeof(kv->key_size) +
             kv->key_size + sizeof(kv->value_size),
         kv->value, kv->value_size);
  write_buffer_offset_ += true_kvsize;
  return 0;
}

// Reads next kv from read_buffer_offset_ from read_buffer_
// and increments the read_buffer_offset_ with the size of kv
tiny_kv_pair *log_handle_t::try_read_from_log_buffer() {
  uint32_t key_size = 0;
  // while (key_size == 0) {
  //   if (read_buffer_offset_ >= LOG_BUFFER_SIZE)
  //     return NULL;
  memcpy(&key_size, read_buffer_ + read_buffer_offset_, sizeof(uint32_t));
  if (!key_size)
    return NULL;

  //     read_buffer_offset_++;
  //     read_padding_++;
  //   }
  // }
  // if (read_buffer_size_ <= 2 * sizeof(uint32_t) + read_buffer_offset_) {
  //   printf("(1) next block\n");
  //   return NULL;
  // }
  struct tiny_kv_pair *kv_pair =
      (struct tiny_kv_pair *)malloc(sizeof(struct tiny_kv_pair));
  memcpy(&kv_pair->key_size, read_buffer_ + read_buffer_offset_,
         sizeof(kv_pair->key_size));
  memcpy(&kv_pair->value_size,
         read_buffer_ + read_buffer_offset_ + sizeof(kv_pair->key_size),
         sizeof(kv_pair->value_size));
  // if (read_buffer_size_ <=
  //     2 * sizeof(uint32_t) + kv_pair->key_size + read_buffer_offset_) {
  //   printf("(2) next block\n");
  //   return NULL;
  // }
  kv_pair->key = (char *)malloc(kv_pair->key_size);
  memcpy(kv_pair->key,
         read_buffer_ + read_buffer_offset_ + sizeof(kv_pair->key_size) +
             sizeof(kv_pair->value_size),
         kv_pair->key_size);
  // if (read_buffer_size_ <= 2 * sizeof(uint32_t) + kv_pair->key_size +
  //                              kv_pair->value_size + read_buffer_offset_) {
  //   printf("(3) next block\n");
  //   return NULL;
  // }

  kv_pair->value = (char *)malloc(kv_pair->value_size);
  memcpy(kv_pair->value,
         read_buffer_ + read_buffer_offset_ + sizeof(kv_pair->key_size) +
             kv_pair->key_size + sizeof(kv_pair->value_size),
         kv_pair->value_size);
  uint32_t true_kvsize = sizeof(kv_pair->key_size) + kv_pair->key_size +
                         sizeof(kv_pair->value_size) + kv_pair->value_size;
  printf("Read buffer offset %ld and got key %s\n", read_buffer_offset_,
         kv_pair->key);

  read_buffer_offset_ += true_kvsize;
  return kv_pair;
}

void log_handle_t::checkpoint_metadata(void *) {}

int log_handle_t::truncate() { return -1; }

// int log_handle_t::replay() {
// }

int log_handle_t::replay() {
  printf("Replay() -> \n");
  allocate_new_log_read_buffer();
  read_padding_ = 0;
  read_buffer_offset_ = 0;
  short exitFlag = 0;
  ssize_t ret = 0;
  int i = 0;
  int keys = 0;
  tiny_kv_pair *kv_pair = NULL;
  off_t l = lseek(fd_, 0, SEEK_SET);
  ret = read(fd_, read_buffer_, read_buffer_size_);
  if (ret == -1) {
    perror("Could not read file");
  }
  while (ret > 0) {
    printf("Read_log_offset %ld and populated read_buffer %d times\n",
           read_log_offset_, i++);
    // get next buffer from log file
    // DEBUG

    while ((kv_pair = try_read_from_log_buffer()) != NULL) {
      keys++;
    }

    printf("Recovered %d kv_pairs\n", keys);

    if (read_buffer_offset_ == 0 && kv_pair == NULL)
      break;

    // //DEBUG
    // break;
    // next buffer from file;
    read_log_offset_ += LOG_BUFFER_SIZE;
    read_buffer_offset_ = 0;
    ret = pread(fd_, read_buffer_, read_buffer_size_, read_log_offset_);
  }
  return -1;
}

log_handle_t::~log_handle_t() {
  close(fd_);
  if (read_buffer_)
    free(read_buffer_);

  if (write_buffer_)
    free(write_buffer_);
}

log_handle_t::log_handle_t(char *location) {
  assert(location != NULL);
  char *log_file = get_filepath(location, LOG_FILE_NAME);
  write_buffer_ = NULL;
  read_buffer_ = NULL;
  if (access(log_file, F_OK) != -1) {
    // recover
    fd_ = open(log_file, O_CREAT | O_RDWR | O_DSYNC | O_DIRECT, 0600);
    if (fd_ < 0) {
      printf("%s: Could not open log file (path: %s)\n", strerror(errno),
             log_file);
    }
    allocate_new_log_write_buffer();
    // replay();
  } else {
    // create a new log
    fd_ = open(log_file, O_CREAT | O_RDWR | O_DSYNC | O_DIRECT, 0600);
    if (fd_ < 0) {
      printf("%s: Could not open log file (path: %s)\n", strerror(errno),
             log_file);
    }
    printf("INFO: Created log file %s\n", log_file);
    // fallocate
    int ret = posix_fallocate(fd_, 0, LOG_FILE_SIZE);
    if (ret < 0) {
      printf("%s: Could not fallocate block_file (path: %s)\n", strerror(errno),
             log_file);
      close(fd_);
      return;
    }
    printf("INFO: fallocated the file\n");
    allocate_new_log_write_buffer();
  }
}
