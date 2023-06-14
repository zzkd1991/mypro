#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ARRAY_LENGTH   10
#define HEAP_SIZE 	10
#define PARENT(i)	(i / 2)
#define LEFT(i)	(i * 2)
#define RIGHT(i)	(i * 2 + 1)

int array[ARRAY_LENGTH] = {10,22,4,48,100,54,8,9,8,9};


void max_heapify(int *A, int i, int heap_size)
{
	int l, r, largest;
	int temp;
	l = LEFT(i);
	r = RIGHT(i);
	if((l <= heap_size) && (A[l] > A[i]))
	{
		largest = l;
	}
	else
	{
		largest = i;
	}
	if((r <= heap_size) && (A[r] > A[largest]))
	{
		largest = r;
	}
	if(largest != i)
	{
		temp = A[i];
		A[i] = A[largest];
		A[largest] = temp;
		max_heapify(A, largest, heap_size);
	}
}

void build_max_heap(int *A)
{
	int i;
	for(i = HEAP_SIZE / 2; i >=1; i--)
	{
		max_heapify(A, i, HEAP_SIZE);
	}
}

void heap_sort(int *A)
{
	int i;
	int temp;
	i = ARRAY_LENGTH;
	int heap_size = HEAP_SIZE;
	build_max_heap(A);
	for(i = ARRAY_LENGTH; i >= 2; i--)
	{
		temp = A[1];
		printf("temp is %d\n", temp);
		A[1] = A[i];
		A[i] = temp;
		heap_size = heap_size - 1;
		max_heapify(A, 1, heap_size);
	}
}

int main(void)
{
	int i;
	int array_temp[ARRAY_LENGTH + 1] = {0};
	for(i = 0; i < ARRAY_LENGTH; i++)
	{
		array_temp[i + 1] = array[i];
	}
	heap_sort(array_temp);
	for(i = 1; i < ARRAY_LENGTH + 1; i++)
	{
		printf("array[%d] is %d\n", i, array_temp[i]);
	}
	return 0;
}