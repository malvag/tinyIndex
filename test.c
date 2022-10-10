#include "tinyblob.h"
#include <assert.h>
#include <stdlib.h>

int main(int argc, char **argv) {
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
  for (int i = 0; i < 1000000; i++) {
    bid_t new_blob_id = tb_allocate_blob();

    assert(!tb_write_blob(new_blob_id, "DATA"));
    char buf[FILE_BLOB_SIZE];
    assert(!tb_read_blob(new_blob_id, buf));

    buf[10] = '\0';
    if (i % 1000 == 0)
      printf("%d %s\n", new_blob_id, buf);
    tb_free_blob(new_blob_id);
  }

  tb_shutdown();
  return 0;
}
