#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int factor;

int findbigone(int *array, int size)
{
	int i;
	int temp;
	for(i = 0; i < size; i++)
	{
		if(array[i] >= temp)
			temp = array[i];
	}
	return temp;
}

int *countsort(int * array, int size)
{
	int i, j;
	int len = findbigone(array, size);
	len++;
	int array1[len];
	//int result[size];
	int *result = malloc(sizeof(int) * size);
	if(result == NULL)
	{
		return NULL;
	}
	//printf("%s, %d\n", __func__, __LINE__);
	memset(array1, 0, len * sizeof(array1[0]));

	for(j = 0; j < size; j++)
		array1[array[j]] = array1[array[j]] + 1;
	//printf("%s, %d\n", __func__, __LINE__);
	for(i = 1; i < len; i++)
	{
		array1[i] = array1[i] + array1[i - 1];
	}
	//printf("%s, %d\n", __func__, __LINE__);
	for(j = size - 1; j >= 0; j--)
	{
		//printf("%s, %d\n", __func__, __LINE__);
		//printf("%s, %d, %d\n", __func__, __LINE__, (array1[array[j]] - 1));
		result[array1[array[j]] - 1 ] = array[j];
		//printf("%s, %d\n", __func__, __LINE__);
		array1[array[j]] = array1[array[j]] - 1;
	}
	return result;
}

int *sor_array_bykey(int *array, int key, int *size, int len)
{
	int i;
	int cout = 0;
	//int result[100] = {0};//这个地方要改
	int *result = NULL;
	int *last_result = NULL; 
	for(i = 0; i < len; i++)
	{
		if((array[i] / factor) == key)
		{
			//result[cout] = array[i];
			cout++;
		}
	}
	if(cout == 0)
	{
		return NULL;
	}
	else
	{
		result = malloc(cout * sizeof(int));
		if(result == NULL)
		{
			return NULL;
		}
		cout = 0;
		for(i = 0; i < len; i++)
		{
			if((array[i] / factor) == key)
			{
				result[cout] = array[i];
				cout++;
			}
		}
	}
	//for(i = 0; i <10; i++)
	//{
	//	printf("result[%d] is %d\n", i, result[i]);
	//}
	//printf("%s, %d\n", __func__, __LINE__);
	*size = cout;
	last_result = countsort(result, cout);
	free(result);
	return last_result;
}

int main(void)
{
	#define ARRAY_LEN	(sizeof(array) / sizeof(array[0]))
	#define BRAN_LEN	(sizeof(bran_key) / sizeof(bran_key[0]))
	int array[] = {1,2,1,99,10, 88, 80,83, 65,67,69, 1000, 301, 220,405,401, 401, 500, 100, 340, 607, 809};
	int bran_key[11] = {0,1,2,3,4,5,6,7,8,9,10};
	//int result[8];
	int *result = NULL;
	int sor_array[ARRAY_LEN] = {0};
	int *p = &sor_array[0];
	int size;
	int i;
	int j;
	factor = findbigone(array, sizeof(array) / sizeof(array[0])) / 10;
	//result = sor_array_bykey(array, 8, &size, sizeof(array) / sizeof(array[0]));
	for(i = 0; i < BRAN_LEN; i ++)
	{
		result = sor_array_bykey(array, bran_key[i], &size, ARRAY_LEN);
		{
			if(result != NULL)
			{
				for(j = 0; j < size; j++)
				{
					//printf("result[%d], %d\n", j, result[j]);
				}
				memcpy(p, result, size * sizeof(int));
				p += size;
				free(result);
			}
		}
	}
	if(sor_array != NULL)
	{
		for(i = 0; i < ARRAY_LEN; i++)
		{
			printf("sor_array[%d], %d\n", i, sor_array[i]);
		}
	}

	return 0;
}	
