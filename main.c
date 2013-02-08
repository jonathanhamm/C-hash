#include "hash.h"
#include <stdio.h>
#include <time.h>

int evil_hash (key_u key)
{
	return 1;
}

int main (void)
{
	int i, j;
	int t = clock();
	int array[20000];
	hash_s *hash = DEFAULT_HASH();
	for (i = 0, j = 0; i < 20000; i++, j++)
	{
		array[j] = i;
		add_record (hash, (key_u)array[j]);
	}	
	for (i = 0; i < 1993; i++)
		if (delete_record (hash, (key_u)array[i]) < 0)
			printf("hello son\n");	
	print_hash (hash);
	printf("Last:%d\n",array[19999]);
	printf("%d,%d\n",hash->size, hash->collisions);
	dealloc_hash (hash);
	printf("total time: %d\n",(int)clock()-t);
}