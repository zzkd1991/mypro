#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ARRAY_LENGTH   10
#define HEAP_SIZE 	10
#define PARENT(i)	(i / 2)
#define LEFT(i)	(i * 2)
#define RIGHT(i)	(i * 2 + 1)

int heap_size = 0;

int heap_maxmum(int *A)//返回S中有最大键字的元素
{
	return A[1];
}

void max_heapify(int *A, int k)
{
	int i;
	for(i = 0; i < heap_size; i++)
	{
		if(A[k] < A[i + 1])
		{
			temp = A[k];
			A[k] = A[i + 1];
			A[i + 1] = temp;
		}
		
	}
}

int heap_extract_max(int *A)//去掉并返回S中的具有最大键字的元素
{
	int max = 0;
	if(heap_size < 1)
	{
		printf("heap underflow\n");
		return -1;
	}
	max = A[1];
	A[1] = A[heap_size];
	heap_size = heap_size - 1;
	max_heapify(A, 1);
	return max;
}

void heap_increase_key(int *A, int i, int key)//将元素i的关键字值增加到key，这里建设key的值不小于i的原关键字值
{
	if(key < A[i])
	{
		printf("new key is smaller than current key\n");
		return;
	}
	A[i] = key;
	while(i > 1 &&  A[PARENT(i)] < A[i])
	{
		temp = A[i];
		A[i] = A[PARENT(i)] 
		A[PARENT(i)] = temp;
		i = PARENT(i);
	}
}

void max_heap_insert(int *A, int key)//把元素key插入到集合A中
{
	heap_size = heap_size + 1;
	A[heap_size] = -100;
	heap_increase_key(A, heap_size, key);
}

int main(void)
{
	int i = 0;
	int array_temp[ARRAY_LENGTH + 1] = {0};
	int array[ARRAY_LENGTH] = {10,22,4,48,100,54,8,9,8,9};
	for(i = 0; i < ARRAY_LENGTH; i++)
	{
		array_temp[i + 1] = array[i];
	}
	for(i = 1; i < ARRAY_LENGTH + 1; i++)
	{
		max_heap_insert(array_temp, array_temp[i]);
	}
	return 0;
}