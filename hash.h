#ifndef __HASH_H
#define __HASH_H
#include <stdint.h>

#define H_TABLE_SIZE 500
#define CACHE_SIZE 64

#define CACHE_DATA_SEGMENT

#define DEFAULT_HASH() alloc_hash((void *)0)

typedef struct hash_s hash_s;
typedef union key_u key_u;

typedef unsigned int (*hash_f) (key_u);

union key_u
{
	int int32;
	unsigned int uint32;
	uint64_t int_value;
	unsigned char array_value[8];
};

struct hash_s
{
	int collisions;
	int size;
	struct record_s *table;
	hash_f hashf;
#if CACHE_SIZE > 0
	struct cache_s *cache;
#endif
};

extern hash_s *alloc_hash (hash_f);
extern int add_record (hash_s *, key_u);
extern int delete_record (hash_s *, key_u);
extern int dealloc_hash (hash_s *);
extern void print_hash (hash_s *);

#endif