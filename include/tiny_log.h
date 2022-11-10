#pragma once

#include "tiny_index.h"

class log_handle_t {
public:
  log_handle_t(char *location);

  void append(struct tiny_kv_pair *pair); // Appends key, value in the log

  void checkpoint_metadata(void *); // checkpoints all metadata of the kv store

  int truncate(); // Truncates the existing log, return 1 on success 0 on
                  // failure

  int replay(); // recover actions from log
  ~log_handle_t();

private:
  int try_write_to_log_buffer(struct tiny_kv_pair *);
  tiny_kv_pair *try_read_from_log_buffer();
  void allocate_new_log_write_buffer();
  void allocate_new_log_read_buffer();

  log_handle_t();
  int fd_;

  off_t write_log_offset_;
  off_t read_log_offset_;

  size_t size_;

  char *read_buffer_;
  size_t read_buffer_size_;
  off_t read_buffer_offset_;
  uint32_t read_padding_;

  char *write_buffer_;
  size_t write_buffer_size_;
  off_t write_buffer_offset_;
};
