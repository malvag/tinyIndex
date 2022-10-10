#include "include/tinyblob.h"
#include "string.h"
#include <assert.h>
#include <stdlib.h>


int main(int argc, char **argv) {
  printf("Starting... Creating 1000 blobs with flush & recoveries and after that we create another 1000 blobs without flushes and recoveries (avg time to complete 16-20 seconds) \n");
  tb_init(argv[1]);
  for (int i = 0; i < 1000; i++) {
    tb_init(argv[1]);
    bid_t new_blob_id = tb_allocate_blob();

    assert(!tb_write_blob(new_blob_id, "DATA"));

    char buf[FILE_BLOB_SIZE];
    assert(!tb_read_blob(new_blob_id, buf));

    buf[10] = '\0';
    if (i % 1000 == 0)
      printf("%d %s\n", new_blob_id, buf);
    // tb_free_blob(new_blob_id);
    tb_shutdown();
  }

  printf("Finished with 1000 flush & recoveries \n");

  tb_init(argv[1]);
  char input[4096];
  input[0] = 'D';
  input[1] = 'A';
  input[2] = 'T';
  input[3] = 'A';
  input[4] = '\0';

  for (int i = 0; i < 1000; i++) {
    bid_t new_blob_id = tb_allocate_blob();
    assert(new_blob_id >= 0);
    tb_write_blob(new_blob_id,input);
    char buf[FILE_BLOB_SIZE];
    tb_read_blob(new_blob_id, buf);

    buf[10] = '\0';
    printf("%d %s\n", new_blob_id, buf);
  }

  tb_shutdown();

  return 0;
}
