#include "tinyblob.h"
#include <assert.h>
#include <stdlib.h>

int main(int argc, char** argv){
    tb_init(argv[1]);
    bid_t new_blob_id = tb_allocate_blob();
    
    char* skata;
    assert(!tb_write_blob(new_blob_id,"SKATOULES"));

    char buf[FILE_BLOB_SIZE];
    assert(!tb_read_blob(new_blob_id,buf));

    buf[10] = '\0';
    printf("%s\n",buf);
    //tb_free_blob(new_blob_id);
    tb_shutdown();
    return 0;
}