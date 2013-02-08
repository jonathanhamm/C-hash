#include <stdio.h>
#include <stdlib.h>

unsigned char mya[] =     {
          0xa0,
          0xa1,
          0xa2,
          0xa3,
          0xa4,
          0xa5,
          0xa6,
          0xa7,
          0xa8,
          0xa9,
          0xaa,
          0xab,
          0xac,
          0xad,
          0xae,
          0xaf
        };

void * mya2[] =     {
          NULL,
          NULL,
          mya,
          NULL
        };

int main(int argc, char**argv)
{
  unsigned long size = sizeof(mya) / sizeof(unsigned char);
  size_t i = 0;

  void * ans = (void*)mya2;
/*Now display the elements of mya by casting/dereferencing the void pointer ans*/
/*Note: You can't access the elements in mya directly, you must access them through the void pointer ans...Good Luck*/
 for (;i < size; i++)
	printf("ans->0x%x\n",((unsigned char *)((void **)ans)[2])[i]); 
 exit(EXIT_SUCCESS);
}
