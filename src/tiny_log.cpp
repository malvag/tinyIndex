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
#include "tiny_index.h"
#include "utilities.h"
class tiny_index;

void *checkpoint_and_truncate(void *arg) {
    sleep(1);
    tiny_index *index = (tiny_index *)arg;
    log_handle_t* log_hdl  =index->get_log_handle();
    pthread_rwlock_t *lock_ = log_hdl->get_lock();
    while (!index->get_index_status_done_()) {
        // checkpoint
        // assert(tb_handle != NULL);
        RWLOCK_WRLOCK(lock_);
        index->persist_unordered_map();
        printf("Persisted %d keys and kv_metadata\n", index->lookup_.size());

        // truncate
        index->get_log_handle()->truncate(index);
        RWLOCK_UNLOCK(lock_);

        sleep(10);
    }
    return NULL;
}

tiny_blob_handle_t* tiny_index::get_blob_handle(){
    return handle_;
}
pthread_rwlock_t *log_handle_t::get_lock() { return lock_; }

void log_handle_t::allocate_new_log_write_buffer() {
    if (write_buffer_ != NULL) free(write_buffer_);
    write_buffer_ = (char *)calloc(1, LOG_BUFFER_SIZE);
    write_buffer_size_ = LOG_BUFFER_SIZE;
    write_buffer_offset_ = 0;
}

void log_handle_t::allocate_new_log_read_buffer() {
    if (read_buffer_ != NULL) free(read_buffer_);
    read_buffer_ = (char *)memalign(FS_LOGICAL_BLK_SIZE, LOG_BUFFER_SIZE);
    read_buffer_size_ = LOG_BUFFER_SIZE;
    read_buffer_offset_ = 0;
}

int log_handle_t::init_rwlock() {
    pthread_rwlock_t *rwlock =
        (pthread_rwlock_t *)malloc(sizeof(pthread_rwlock_t));
    pthread_rwlockattr_t *rwlock_attr =
        (pthread_rwlockattr_t *)malloc(sizeof(pthread_rwlock_t));
    pthread_rwlockattr_init(rwlock_attr);
    pthread_rwlockattr_setpshared(rwlock_attr, PTHREAD_PROCESS_SHARED);
    if (pthread_rwlock_init(rwlock, rwlock_attr) == 0) {
        lock_ = rwlock;
        lock_attr_ = rwlock_attr;
        return 0;
    }
    return -1;
}

void log_handle_t::destroy_rwlock() {
    pthread_rwlockattr_destroy(lock_attr_);
    free(lock_attr_);
    pthread_rwlock_destroy(lock_);
    free(lock_);
}

void log_handle_t::append(tiny_kv_pair *kv) {
    // since we have O_APPEND we get the offset
    ssize_t amount_written, position;

    /* Write out the data.  */
    RWLOCK_WRLOCK(lock_);
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
        // printf("Persisted kvs into log\n");
        allocate_new_log_write_buffer();
        if (try_write_to_log_buffer(kv) == -1) return;
    } else {
        // printf("Stored kv[%s:%s] into buffer, offset %zu\n", kv->key, kv->value,
        //        write_buffer_offset_);
    }
    RWLOCK_UNLOCK(lock_);
}

// Writes kv to write_buffer_ at write_buffer_offset_
// and increments the write_buffer_offset_ with the size of kv
int write_kv_to_buffer(char *buf, uint32_t buf_size, off_t *offset,
                       tiny_kv_pair *kv) {
    uint32_t true_kvsize = sizeof(kv->key_size) + kv->key_size +
                           sizeof(kv->value_size) + kv->value_size;
    if (buf_size <= true_kvsize + *offset) return -1;
    // construct tiny_kv_pair buffer
    memcpy(buf + *offset, &kv->key_size, sizeof(kv->key_size));

    memcpy(buf + *offset + sizeof(kv->key_size), &kv->value_size,
           sizeof(kv->value_size));

    memcpy(buf + *offset + sizeof(kv->key_size) + sizeof(kv->value_size),
           kv->key, kv->key_size);

    memcpy(buf + *offset + sizeof(kv->key_size) + kv->key_size +
               sizeof(kv->value_size),
           kv->value, kv->value_size);
    *offset += true_kvsize;
    return 0;
}

// Writes kv to write_buffer_ at write_buffer_offset_
// and increments the write_buffer_offset_ with the size of kv
int log_handle_t::try_write_to_log_buffer(tiny_kv_pair *kv) {
    return write_kv_to_buffer(write_buffer_, write_buffer_size_,
                              &write_buffer_offset_, kv);
}

// Reads next kv from read_buffer_offset_ from read_buffer_
// and increments the read_buffer_offset_ with the size of kv
tiny_kv_pair *log_handle_t::try_read_from_log_buffer() {
    uint32_t key_size = 0;
    memcpy(&key_size, read_buffer_ + read_buffer_offset_, sizeof(uint32_t));
    if (!key_size) return NULL;

    struct tiny_kv_pair *kv_pair =
        (struct tiny_kv_pair *)malloc(sizeof(struct tiny_kv_pair));
    memcpy(&kv_pair->key_size, read_buffer_ + read_buffer_offset_,
           sizeof(kv_pair->key_size));
    memcpy(&kv_pair->value_size,
           read_buffer_ + read_buffer_offset_ + sizeof(kv_pair->key_size),
           sizeof(kv_pair->value_size));
    kv_pair->key = (char *)malloc(kv_pair->key_size);
    memcpy(kv_pair->key,
           read_buffer_ + read_buffer_offset_ + sizeof(kv_pair->key_size) +
               sizeof(kv_pair->value_size),
           kv_pair->key_size);

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

// is lock-protected from calller
int log_handle_t::truncate(tiny_index *index) {
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
    char *write_shifted_buffer =
        (char *)memalign(FS_LOGICAL_BLK_SIZE, LOG_FILE_SIZE);
    memset(write_shifted_buffer, 0, LOG_FILE_SIZE);
    uint32_t write_shifted_buffer_size = LOG_FILE_SIZE;
    off_t write_shifted_buffer_offset = 0;
    if (ret == -1) {
        perror("Could not read file");
    }
    while (ret > 0) {
        // printf("Read_log_offset %ld and populated read_buffer %d times\n",
        //        read_log_offset_, i++);

        // if value in kv is NULL, then populate a buffer with every key next to
        // it and shift to file's beginning

        while ((kv_pair = try_read_from_log_buffer()) != NULL) {
            char *val_buf = index->get(kv_pair->key);
            if (val_buf == NULL) {
                // index->put(kv_pair->key, kv_pair->value);
                // put this buffer and the rest back to the beggining of the log
                write_kv_to_buffer(write_shifted_buffer,
                                   write_shifted_buffer_size,
                                   &write_shifted_buffer_offset, kv_pair);
            }
            // free(kv_pair);
            // free(kv_pair->key);
            // free(kv_pair->value);
        }

        printf("Recovered %d kv_pairs\n", keys);

        if (read_buffer_offset_ == 0 && kv_pair == NULL) break;

        // get next buffer from log file
        read_log_offset_ += LOG_BUFFER_SIZE;
        read_buffer_offset_ = 0;
        ret = pread(fd_, read_buffer_, read_buffer_size_, read_log_offset_);
    }
    ret = pwrite(fd_, write_shifted_buffer, write_shifted_buffer_size, 0);
    if (ret != write_shifted_buffer_size) {
        perror("SKATOULES");
        free(write_shifted_buffer);
        return -1;
    }
    free(write_shifted_buffer);
    return 0;
}

int log_handle_t::replay(tiny_index *index) {
    RWLOCK_WRLOCK(lock_);
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

        while ((kv_pair = try_read_from_log_buffer()) != NULL) {
            char *val_buf = index->get(kv_pair->key);
            if (val_buf == NULL) {
                index->put(kv_pair->key, kv_pair->value);
            }
            free(kv_pair);
            free(kv_pair->key);
            free(kv_pair->value);
        }

        printf("Recovered %d kv_pairs\n", keys);

        if (read_buffer_offset_ == 0 && kv_pair == NULL) break;

        // get next buffer from log file
        read_log_offset_ += LOG_BUFFER_SIZE;
        read_buffer_offset_ = 0;
        ret = pread(fd_, read_buffer_, read_buffer_size_, read_log_offset_);
    }
    RWLOCK_UNLOCK(lock_);
    return 0;
}

int log_handle_t::log_read() {
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

        while ((kv_pair = try_read_from_log_buffer()) != NULL) {
            keys++;
        }

        printf("Recovered %d kv_pairs\n", keys);

        if (read_buffer_offset_ == 0 && kv_pair == NULL) break;

        // get next buffer from log file
        read_log_offset_ += LOG_BUFFER_SIZE;
        read_buffer_offset_ = 0;
        ret = pread(fd_, read_buffer_, read_buffer_size_, read_log_offset_);
    }
    return -1;
}

log_handle_t::~log_handle_t() {
    RWLOCK_WRLOCK(lock_);
    close(fd_);
    if (read_buffer_) free(read_buffer_);

    if (write_buffer_) free(write_buffer_);
    RWLOCK_UNLOCK(lock_);
    destroy_rwlock();
}

log_handle_t::log_handle_t(char *location, tiny_index *index) {
    init_rwlock();
    RWLOCK_WRLOCK(lock_);
    assert(location != NULL);
    char *log_file = get_filepath(location, LOG_FILE_NAME);
    write_buffer_ = NULL;
    read_buffer_ = NULL;
    write_log_offset_ = 0;

    int ret =
        pthread_create(&gc_thread_id_, NULL, checkpoint_and_truncate, index);
    if (ret) {
        printf("Error: creating gc thread\n");
        exit(EXIT_FAILURE);
    }

    if (access(log_file, F_OK) != -1) {
        // recover
        fd_ = open(log_file, O_CREAT | O_RDWR | O_DSYNC | O_DIRECT, 0600);
        if (fd_ < 0) {
            printf("%s: Could not open log file (path: %s)\n", strerror(errno),
                   log_file);
        }
        allocate_new_log_write_buffer();
        write_log_offset_ = index->get_blob_handle()->log_offset;
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
            printf("%s: Could not fallocate block_file (path: %s)\n",
                   strerror(errno), log_file);
            close(fd_);
            return;
        }
        printf("INFO: fallocated the file\n");
        allocate_new_log_write_buffer();
        write_log_offset_ = 0;
    }
    RWLOCK_UNLOCK(lock_);
}
