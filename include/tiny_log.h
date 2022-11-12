#pragma once

#include "tiny_index.h"
class tiny_index;
class log_handle_t {
   public:
    log_handle_t(char *location);

    void append(struct tiny_kv_pair *pair);  // Appends key, value in the log

    int truncate(tiny_index *index);  // Truncates the existing log, return 1 on
                                      // success 0 on failure
    int log_read();                   // debug function only
    int replay(tiny_index *index);

    int replay();  // recover actions from log

    pthread_rwlock_t* get_lock();
    ~log_handle_t();
    log_handle_t(char *location, tiny_index *index);

   private:
    int init_rwlock();
    void destroy_rwlock();
    int try_write_to_log_buffer(struct tiny_kv_pair *);
    tiny_kv_pair *try_read_from_log_buffer();
    void allocate_new_log_write_buffer();
    void allocate_new_log_read_buffer();

    pthread_t gc_thread_id_;
    pthread_rwlock_t *lock_;
    pthread_rwlockattr_t *lock_attr_;
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
