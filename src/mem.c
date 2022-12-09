#include <unistd.h>
#include <stddef.h>

#include "mem_internals.h"
#include "mem.h"
#include "util.h"

void debug_block(struct block_header* b, const char* fmt, ... );
void debug(const char* fmt, ... );

extern inline block_size size_from_capacity( block_capacity cap );
extern inline block_capacity capacity_from_size( block_size sz );

static bool            block_is_big_enough( size_t query, struct block_header* block ) { return block->capacity.bytes >= query; }
static size_t          pages_count   ( size_t mem )                      { return mem / getpagesize() + ((mem % getpagesize()) > 0); }
static size_t          round_pages   ( size_t mem )                      { return getpagesize() * pages_count( mem ) ; }

static void block_init( void* restrict addr, block_size block_sz, void* restrict next ) {
  *((struct block_header*)addr) = (struct block_header) {
    .next = next,
    .capacity = capacity_from_size(block_sz),
    .is_free = true
  };
}

static size_t region_actual_size( size_t query ) { return size_max( round_pages( query ), REGION_MIN_SIZE ); }

extern inline bool region_is_invalid( const struct region* r );



static void* map_pages(void const* addr, size_t length, int additional_flags) {
  return mmap( (void*) addr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | additional_flags , 0, 0 );
}

static struct region alloc_region  ( void const * addr, size_t query ) {
    size_t check_query = region_actual_size(query);
    struct region region = {.addr = map_pages(addr, check_query, MAP_FIXED_NOREPLACE),
            .size = check_query, .extends = true};
    if(region.addr == MAP_FAILED){
        region.addr = map_pages(addr, check_query, 0);
        if(region.addr == MAP_FAILED) {
            return REGION_INVALID;
        }
    }
    block_init(region.addr, (block_size){.bytes = check_query}, NULL);

    return region;
    ///
}

static void* block_after( struct block_header const* block )         ;

bool is_inited = false;

void* heap_init( size_t initial ) {
  const struct region region = alloc_region( HEAP_START, initial );
  if ( region_is_invalid(&region) ) return NULL;
  is_inited = true;
  return region.addr;
}

#define BLOCK_MIN_CAPACITY 24


static bool block_splittable( struct block_header* restrict block, size_t query) {
  return block-> is_free && query + offsetof( struct block_header, contents ) + BLOCK_MIN_CAPACITY <= block->capacity.bytes;
}

static bool split_if_too_big( struct block_header* block, size_t query ){
    size_t check_query = size_max(query, BLOCK_MIN_CAPACITY);
    if(block != NULL && block_splittable(block, check_query)){
        block_capacity full_capacity = block->capacity;
        block->capacity = (block_capacity){.bytes = check_query};
        struct block_header* header = (struct block_header*) block_after(block);
        block_init(header, (block_size){.bytes = full_capacity.bytes - (check_query + offsetof(struct block_header, contents))},
                NULL);
        block->next = header;
        return true;
    }
    return false;
    ///
}


static void* block_after( struct block_header const* block )              {
  return  (void*) (block->contents + block->capacity.bytes);
}
static bool blocks_continuous (
                               struct block_header const* fst,
                               struct block_header const* snd ) {
  return (void*)snd == block_after(fst);
}

static bool mergeable(struct block_header const* restrict fst, struct block_header const* restrict snd) {
  return fst->is_free && snd->is_free && blocks_continuous( fst, snd ) ;
}

static bool try_merge_with_next( struct block_header* block ) {
    if(block != NULL && block->next != NULL && mergeable(block, block->next)){
        block->capacity.bytes = block->capacity.bytes + size_from_capacity(block->next->capacity).bytes;
        block->next = block->next->next;
        return true;
    }
    return false;
    ///
}


struct block_search_result {
  enum {BSR_FOUND_GOOD_BLOCK, BSR_REACHED_END_NOT_FOUND, BSR_CORRUPTED} type;
  struct block_header* block;
};


static struct block_search_result find_good_or_last  ( struct block_header* restrict block, size_t sz )    {
    while(block){
        while (try_merge_with_next(block));
        if (block_is_big_enough(sz, block) && block->is_free) {
            split_if_too_big(block, sz);
            return (struct block_search_result) {.block = block, .type = BSR_FOUND_GOOD_BLOCK};
        }
        if(block->next == NULL) break;
        block = block->next;
    }
    return (struct block_search_result){.block = block, .type = BSR_REACHED_END_NOT_FOUND};
    ///
}


static struct block_search_result try_memalloc_existing ( size_t query, struct block_header* block ) {
    struct block_search_result new_block = find_good_or_last(block, query);
    if(new_block.block && new_block.type == BSR_FOUND_GOOD_BLOCK) new_block.block->is_free = false;
    return new_block;
    ///
}


static struct block_header* grow_heap( struct block_header* restrict last, size_t query ) {
    if(last){
        struct region new_region = alloc_region(block_after(last), query);
        if(new_region.addr == NULL) return NULL;
        last->next = (struct block_header*) new_region.addr;
        return (struct block_header*) new_region.addr;
    }
    return NULL;
    ///
}


static struct block_header* memalloc( size_t query, struct block_header* heap_start) {
    if(heap_start == NULL) return NULL;
    struct block_search_result block = try_memalloc_existing(query, heap_start);
    switch (block.type) {
        case BSR_FOUND_GOOD_BLOCK:{
            block.block->is_free = false;
            return block.block;
        }
        case BSR_REACHED_END_NOT_FOUND:{
            if(grow_heap(block.block, query)){
                return try_memalloc_existing(query, block.block).block;
            }
        }
        case BSR_CORRUPTED:{
            return NULL;
        }
    }
    return NULL;
    ///
}

void* _malloc( size_t query ) {
  if(!is_inited) heap_init(REGION_MIN_SIZE);
  struct block_header* const addr = memalloc( query, (struct block_header*) HEAP_START );
  if (addr) return addr->contents;
  else return NULL;
}

static struct block_header* block_get_header(void* contents) {
  return (struct block_header*) (((uint8_t*)contents)-offsetof(struct block_header, contents));
}

void _free( void* mem ) {
  if (!mem) return ;
  struct block_header* header = block_get_header( mem );
  header->is_free = true;
  while(try_merge_with_next(header));
  ///
}
