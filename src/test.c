#include "test.h"
#include "mem_internals.h"
#include "mem.h"
#include "util.h"

static struct block_header* block_get_header(void* contents) {
    return (struct block_header*) (((uint8_t*)contents)-offsetof(struct block_header, contents));
}

bool test1(){
    void* first_malloc = _malloc(123);
    void* second_malloc = _malloc(21);

    if(first_malloc == NULL || second_malloc == NULL){
        return false;
    }

    struct block_header* first_malloc_block_header = block_get_header(first_malloc);
    struct block_header* second_malloc_block_header = block_get_header(second_malloc);

    if(first_malloc_block_header == NULL || second_malloc_block_header == NULL){
        return false;
    }

    if(first_malloc_block_header->capacity.bytes == 123 && second_malloc_block_header->capacity.bytes == 24 &&
        !first_malloc_block_header->is_free && !second_malloc_block_header->is_free){
//        _free(first_malloc);
//        _free(second_malloc);
        return true;
    }
    printf("5\n");
//    _free(first_malloc);
//    _free(second_malloc);
    return false;
}

bool test2(){
    void* first_malloc = _malloc(123);
    void* second_malloc = _malloc(21);

    struct block_header* first_malloc_block_header = block_get_header(first_malloc);
    struct block_header* second_malloc_block_header = block_get_header(second_malloc);

    _free(second_malloc);
    return second_malloc_block_header->is_free && !first_malloc_block_header->is_free;
}

bool test3(){
    void* first_malloc = _malloc(123);
    void* second_malloc = _malloc(21);
    void* third_malloc = _malloc(221);

    struct block_header* first_malloc_block_header = block_get_header(first_malloc);
    struct block_header* second_malloc_block_header = block_get_header(second_malloc);
    struct block_header* third_malloc_block_header = block_get_header(third_malloc);

    _free(first_malloc);
    _free(second_malloc);
    return first_malloc_block_header->is_free && second_malloc_block_header->is_free && !third_malloc_block_header->is_free;
}

bool test4(){
    void* first_malloc = _malloc(16000);

    struct block_header* first_malloc_block_header = block_get_header(first_malloc);

    if(!first_malloc_block_header->is_free && first_malloc_block_header->capacity.bytes == 16000){
//        _free(first_malloc);
        return true;
    }
//    _free(first_malloc);
    return false;
}

bool test5(){
    void* first_malloc = _malloc(9000);
    void* second_malloc = _malloc(16000);

    struct block_header* first_malloc_block_header = block_get_header(first_malloc);
    struct block_header* second_malloc_block_header = block_get_header(second_malloc);

    if(!first_malloc_block_header->is_free && !second_malloc_block_header->is_free &&
        first_malloc_block_header->capacity.bytes == 9000 && second_malloc_block_header->capacity.bytes == 16000){
        return true;
    }
    return false;
}

bool tests(){
    if(!test1()){
        printf("first test is failed\n");
        return false;
    }
    else{
        printf("first test is passed\n");
    }
    if(!test2()){
        printf("second test is failed\n");
        return false;
    }
    else{
        printf("second test is passed\n");
    }
    if(!test3()){
        printf("third test is failed\n");
        return false;
    }
    else{
        printf("third test is passed\n");
    }
    if(!test4()){
        printf("fourth test is failed\n");
        return false;
    }
    else{
        printf("fourth test is passed\n");
    }
    if(!test5()){
        printf("fifth test is failed\n");
        return false;
    }
    else{
        printf("fifth test is passed\n");
    }
    return true;
}
