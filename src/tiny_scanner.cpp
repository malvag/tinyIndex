#include "tiny_scanner.h"

#include <cstdint>
#include <string.h>
int scanner_handle_t::go_to_next() {
  if (iterator_ != map_->end()) {
    iterator_++;
    return 0;
  }
  return -1;
}

scanner_handle_t::scanner_handle_t(
    std::unordered_map<std::string, struct lookup_index_node> map) {

  map_ = new std::map<std::string, struct lookup_index_node>(map.begin(),
                                                             map.end());
  opened_blob_buffer_ = (char *)malloc(FILE_BLOB_SIZE);
  opened_blob_ = FIRST_DATA_BLOB;
  if (tb_read_blob(opened_blob_, opened_blob_buffer_) < 0) {
    printf("ERROR: could not read blob for scanner\n");
    exit(EXIT_FAILURE);
  }
  iterator_ = map_->begin();
};

char *scanner_handle_t::get_key() {
  if (!iterator_->first.empty())
    return strdup(iterator_->first.c_str());

  return NULL;
};

char *scanner_handle_t::get_value() {

  if (iterator_->second.blob_id != opened_blob_) {
    memset(opened_blob_buffer_, 0, FILE_BLOB_SIZE);
    tb_read_blob(iterator_->second.blob_id, opened_blob_buffer_);
  }
  uint32_t value_size;
  uint32_t key_size = iterator_->first.size();

  memcpy(&value_size,
         opened_blob_buffer_ + iterator_->second.blob_offset + sizeof(uint32_t),
         sizeof(uint32_t));

  char *value = (char *)malloc(value_size);
  memcpy(value,
         opened_blob_buffer_ + iterator_->second.blob_offset +
             2 * sizeof(uint32_t) + key_size,
         value_size);
  return value;
}

int tiny_index::erase(char *key) { return !lookup_.erase(key); }

scanner_handle_t::~scanner_handle_t(){
  free(opened_blob_buffer_);
  delete map_;
}

scanner_handle_t *tiny_index::scan_init(void) {
  scanner_handle_t *scanner = new scanner_handle_t(lookup_);
  return scanner;
}

char *tiny_index::get_scan_key(scanner_handle_t *scanner) {
  return scanner->get_key();
}

char *tiny_index::get_scan_value(scanner_handle_t *scanner) {
  return scanner->get_value();
}

int tiny_index::close_scanner(scanner_handle_t *scanner) {

  delete scanner;
  scanner = NULL;
  return 0;
}

int tiny_index::get_next(scanner_handle_t *scanner) {
  return scanner->go_to_next();
}
