#include "string.h"
#include <stdlib.h>
#include <stdio.h>

struct kv{
    int size;
    char* key;
};

int main(){
    struct kv* pair = (struct kv*)malloc(sizeof(struct kv));
    pair->key = strdup("malvag");
    pair->size = strlen(pair->key);

    char* buffer = (char*) malloc (1024);
    memcpy(buffer+4,&pair->size,sizeof(int));
    memcpy(buffer+4 + sizeof(int),pair->key,pair->size);
    free(pair->key);
    free(pair);
    pair = NULL;

    struct kv* new_pair = (struct kv*)malloc(sizeof(struct kv));
    memcpy(&new_pair->size,buffer+4,sizeof(int));
    new_pair->key = (char*)malloc(new_pair->size);
    memcpy(new_pair->key,buffer+4 + sizeof(int),new_pair->size);

    printf("%s\n",new_pair->key);
    return 0;

}