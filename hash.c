#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHAIN_NODE_SIZE 8 

struct chain_s
{
	unsigned char mem_tracker;
	key_u keys[CHAIN_NODE_SIZE];
	struct chain_s *next;
	struct chain_s *prev;
};

struct cache_s
{
	uint64_t mem_tracker;
	struct chain_s cache[CACHE_SIZE];
};

struct record_s
{
    key_u key;
	union {
		long is_occupied;
		struct chain_s *sparse_nodes;
	} status;
	struct chain_s *full_nodes;
};

static unsigned int default_hash_f (key_u);
static struct chain_s *chain_s_ (hash_s *);
static struct chain_s **search_chain(struct chain_s **, key_u);
static void push_node (struct chain_s **, struct chain_s *);
static struct chain_s *remove_node (struct chain_s **);
static struct chain_s **delete_chained_key (hash_s *, struct chain_s **, key_u);
static struct chain_s *getMemory (void);

#if CACHE_SIZE > 0
static inline void release_chain (hash_s *, struct chain_s *);
#endif

#ifdef CACHE_DATA_SEGMENT
struct cache_s cache;
#endif

static char *dectobin(unsigned char byte)
{
	static char array[9];
	array[8] = '\0';
	if (byte == 0U)
	{
		memset(array,'0',8);
		return array;
	}
	if (byte == 1U)
	{
		array[7] = '1';
		memset(array,'0',7);
	}
	int i = (int)ceil(log(byte)/log(2));
	//array[(int)ceil(log(byte)/log(2))] = '\0';
	unsigned int quotient = (unsigned int)byte;
	for (; i >= 0; quotient/=2,i--){
		if (!(quotient%2))
			array[i] = '0';
		else
			array[i]= '1';
	}
	int j;
	for (j = 0; j < i+1; j++)
		array[j] = '0';
	return array;
}

hash_s *alloc_hash (hash_f hfunction)
{
	hash_s *hash;
	hash = NULL;
	if ((hash = calloc(1,sizeof(*hash))) == NULL)
		return NULL;
	hash->table = NULL;
	if ((hash->table = calloc(H_TABLE_SIZE,sizeof(*hash->table))) == NULL) {
		free (hash);
		return NULL;
	}
	if (hfunction == NULL)
		hash->hashf = default_hash_f;
	else
		hash->hashf = hfunction;
#if CACHE_SIZE > 0 && !defined(CACHE_DATA_SEGMENT)
	hash->cache = NULL;
	if ((hash->cache = calloc(CACHE_SIZE,sizeof(*hash->cache))) == NULL) {
		free (hash->table);
		free (hash);
		return NULL;
	}
#elif CACHE_SIZE > 0
	hash->cache = &cache;
	hash->cache->mem_tracker = UINT64_MAX;
#endif
	return hash;
}

int add_record (hash_s *hash, key_u key)
{
	unsigned int index;
	struct chain_s *sparse_node;
	struct record_s *record;
	if (hash == NULL)
		return -1;
	index = hash->hashf(key);
	if (index >= H_TABLE_SIZE)
		return -1;
	record = &hash->table[index]; 
	if (!record->status.is_occupied) {
		record->key = key;
		record->status.is_occupied = 1;
	}
	else if (record->status.is_occupied == 1) {	
		sparse_node = NULL;
		if (record->key.int_value == key.int_value)
			return 0;
		if ((sparse_node = chain_s_(hash)) == NULL)
			return -1;
		sparse_node->mem_tracker = 0xFE;
		sparse_node->keys[0] = key;
		push_node (&record->status.sparse_nodes, sparse_node);
		hash->collisions++;
	}
	else {
		if (search_chain(&record->status.sparse_nodes, key) != NULL || search_chain(&record->full_nodes, key) != NULL)
			return 0;
		sparse_node = record->status.sparse_nodes;
		sparse_node->keys[ffsl(sparse_node->mem_tracker)-1] = key;
		sparse_node->mem_tracker &= ~(unsigned char)(1 << (ffsl(sparse_node->mem_tracker)-1));
		if (sparse_node->mem_tracker == 0) {
			printf("NODE SWAP: sparse ---> full\n");
			push_node (&record->full_nodes, remove_node (&record->status.sparse_nodes));
			sparse_node = NULL;
			if ((sparse_node = chain_s_(hash)) == NULL)
				return -1;
			push_node (&record->status.sparse_nodes, sparse_node);
		}
		hash->collisions++;
	}
	hash->size++; 
	return 1;
}

int delete_record (hash_s *hash, key_u key)
{
	unsigned int got_full;
	int index;
	struct chain_s **temp_node;
	struct record_s *record;
	if (hash == NULL)
		return -1;
	got_full = hash->hashf(key);
	index = got_full;
	if (got_full >= H_TABLE_SIZE)
		return -1;
	record = &hash->table[got_full];
	if (!record->status.is_occupied)
		return 0;
	if (record->key.int_value == key.int_value) {
		if (record->status.is_occupied == 1){
			record->status.is_occupied = 0; 
		}
		else {
			printf("1 delete hash record: %d\n",index);
			if (record->status.sparse_nodes == NULL) {
				for(;;);
				if (record->full_nodes == NULL)
					record->status.is_occupied = 1;
				else
					push_node (&record->status.sparse_nodes, remove_node (&record->full_nodes));
			}
			key = record->status.sparse_nodes->keys[ffsl((unsigned char)~record->status.sparse_nodes->mem_tracker)-1];			
			temp_node = delete_chained_key (hash, &record->status.sparse_nodes, key);
			record->key = key;
		}
	}
	else if (record->status.is_occupied > 1) {
		got_full = 0;		
		temp_node = search_chain (&record->status.sparse_nodes, key);
		if (temp_node == NULL)
		{
			temp_node = search_chain (&record->full_nodes, key);			
			if (temp_node == NULL)
				return -1;
			got_full = 1;
		}		
		temp_node = delete_chained_key (hash, temp_node, key);
		if (got_full && temp_node != NULL) {
			printf("NODE SWAP: full ---> sparse\n");
			push_node (&record->status.sparse_nodes, remove_node (temp_node));
		}
		else if (temp_node == NULL) {
			for(;;)	printf("Setting to one from list deletion: %d\n",index);
			record->status.is_occupied = 1;
		}	
	}
	return 0;
}

static struct chain_s **search_chain (struct chain_s **head, key_u key)
{
	unsigned char i, j;
	struct chain_s *backup;
	struct chain_s **rvalue;
	for (backup = *head; *head != NULL; *head = (*head)->next) {
		for (j =  ~(*head)->mem_tracker, i = ffsl(j)-1; j != 0; j &= ~(1 << i), i = ffsl(j)-1) 
			if ((*head)->keys[i].int_value == key.int_value) {
				if ((*head)->prev != NULL)
					rvalue = &(*head)->prev->next;
				else 
					rvalue = head;
				*head = backup;
				return rvalue;
			}
	}
	*head = backup;
	return NULL;
}

static void push_node (struct chain_s **head, struct chain_s *node)
{
	if (node == NULL)
		return;
	if (*head == (struct chain_s *)1)
		*head = NULL;
	node->next = *head;
	node->prev = NULL;
	if (*head != NULL)
		(*head)->prev = node;
	*head = node;
}

static struct chain_s *remove_node (struct chain_s **location)
{
	struct chain_s *rnode;
	rnode = *location;
	if ((*location)->prev == NULL) {
		*location = (*location)->next;
		if (*location != NULL)
			(*location)->prev = NULL;
		return rnode;
	} 
	else 
		rnode->prev->next = rnode->next;
	if (rnode->next != NULL)
		rnode->next->prev = rnode->prev;
	return rnode;
}

static struct chain_s **delete_chained_key (hash_s *hash, struct chain_s **chain, key_u key)
{
	unsigned char i, j;
	struct chain_s *backup;
	struct chain_s *target;
	struct chain_s **rvalue;
	struct chain_s *iterator;
	for (backup = *chain; *chain != NULL; *chain = (*chain)->next) {
		for (j = ~(*chain)->mem_tracker, i = ffsl(j)-1; j != 0; j &= ~(1 << i), i = ffsl(j)-1) {
			if ((*chain)->keys[i].int_value == key.int_value) {				
				(*chain)->mem_tracker |= (1 << i);
				if ((*chain)->mem_tracker == 0xFF) {
					printf("------------------------------------FREE TIME------------------------------------\n");
					target = remove_node (chain);
					if (backup != target)
						*chain = backup;
					else if (backup->next != NULL)
						*chain = backup->next;
					else
						chain = NULL;
					if (target) {
#if CACHE_SIZE > 0	
						release_chain (hash,target);
#else
						free(target);
#endif
					}
					return chain;
				}
				rvalue = chain;
				*chain = backup;
				return rvalue;
			}
		}
	}
	*chain = backup;
	return chain;
}

int dealloc_hash (hash_s *hash)
{
	int i;
	struct chain_s *temp;
	struct chain_s *iterator;
	if (hash == NULL)
		return -1;
	for (i = 0; i < H_TABLE_SIZE; i++)
	{
		if (hash->table[i].status.is_occupied > 1)
		{
			for (iterator = hash->table[i].status.sparse_nodes; iterator != NULL;)
			{
				temp = iterator;
				iterator = iterator->next;
#if CACHE_SIZE > 0
				printf("ADDRESS: %lu,%lu,%lu\n",hash->cache,hash->cache+CACHE_SIZE,((unsigned long)temp - (unsigned long)hash->cache)/sizeof(*temp));
				if (temp < hash->cache->cache || temp > hash->cache->cache + CACHE_SIZE*sizeof(struct chain_s))
#endif	
				free (temp);
			}
			if (hash->table[i].full_nodes != NULL)
			for (iterator = hash->table[i].full_nodes; iterator != NULL;)
			{
				temp = iterator;
				iterator = iterator->next;
#if CACHE_SIZE > 0
				printf("ADDRESS: %lu,%lu,%lu\n",hash->cache,hash->cache+CACHE_SIZE,((unsigned long)temp - (unsigned long)hash->cache)/sizeof(*temp));

				if (temp < hash->cache->cache || temp > hash->cache->cache + CACHE_SIZE*sizeof(struct chain_s))
#endif	
				free (temp);
			}
		}
	}
#if CACHE_SIZE > 0 && !defined(CACHE_DATA_SEGMENT)
	free (hash->cache);
#endif
	free (hash->table);
	free (hash);
	hash = NULL;
	return 1;
}

static struct chain_s *chain_s_ (hash_s *hash)
{
	struct chain_s *new;
#if CACHE_SIZE > 0
	int set_bit_index;
	set_bit_index = ffsl (hash->cache->mem_tracker)-1;
	if (set_bit_index >= 0) {
		hash->cache->mem_tracker &= ~((uint64_t)1 << set_bit_index);
		return &hash->cache->cache[set_bit_index];
	}
#endif
	new = NULL;
	if ((new = calloc(1,sizeof(*new))) == NULL)
		return NULL;
	new->mem_tracker = 0xFF;
	return new;
}

#if CACHE_SIZE > 0
static inline void release_chain (hash_s *hash, struct chain_s *chain)
{
	if (chain > hash->cache->cache && chain < hash->cache->cache + CACHE_SIZE*sizeof(struct chain_s))
		hash->cache->mem_tracker |= ((uint64_t)1 << (((unsigned long)chain - (unsigned long)hash->cache)/sizeof(*chain)));
	else 
		free (chain);
}
#endif

void print_hash (hash_s *hash)
{
	unsigned int i;
	unsigned char j, k;
	struct chain_s *iterator;
	if (hash == NULL)
		return;
	for (i = 0; i < H_TABLE_SIZE; i++)
	{
		if (hash->table[i].status.is_occupied > 0)
		{
			printf("\n---------------RECORD---------------\n");
			printf("First Key at %d is: %d\n", i, hash->table[i].key.int32);
			if (hash->table[i].status.is_occupied > 1)
			{
				for (iterator = hash->table[i].status.sparse_nodes; iterator != NULL; iterator = iterator->next)
				{
					printf("---SPARSE NODE---,%u,%s\n",iterator->mem_tracker,dectobin(iterator->mem_tracker));
					for (k = ~iterator->mem_tracker, j = ffsl(k)-1; k != 0; k &= ~(1 << j), j = ffsl(k)-1)
						printf("\t---Key in Sparse List %d\n", iterator->keys[j].int32);
				}
				if (hash->table[i].full_nodes != NULL)
				{
					printf("   |\n   |\n");
					for (iterator = hash->table[i].full_nodes; iterator != NULL; iterator = iterator->next)
					{
						printf("---FULL NODE---%u,%s\n",iterator->mem_tracker,dectobin(iterator->mem_tracker));
						for (k = ~iterator->mem_tracker, j = ffsl(k)-1; k != 0; k &= ~(1 << j), j = ffsl(k)-1)
							printf("\t---Key in Full List %d\n", iterator->keys[j].int32);
					}
				}
			}
		}
	}
}

/*Default Hash Function*/
static unsigned int default_hash_f (key_u key)
{
	return key.int_value % H_TABLE_SIZE;
}