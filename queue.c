#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define QUEUE_LEN 	3

struct queue{
	int head;
	int tail;
	int head_flag;
	int tail_flag;
	char *queue_data;
};



struct queue queueall[QUEUE_LEN];

void queue_init(struct queue* queue_head)
{
	queue_head->head = 1;
	queue_head->tail = 1;
	queue_head->head_flag = 1;
	queue_head->tail_flag = 1;
	queue_head->queue_data = NULL;
}

int queue_is_full(struct queue *queue_head)
{
	queue_head->head_flag = queue_head->head_flag + 1;
	if(queue_head->head_flag - queue_head->head == QUEUE_LEN + 1)
	{
		printf("queue is full!\n");
		return 1;
	}
	else
	{
		return 0;
	}
}

int queue_is_empty(struct queue *queue_head)
{
	queue_head->tail_flag = queue_head->tail_flag + 1;
	if(queue_head->tail_flag == queue_head->head_flag + 1)
	{
		printf("queue is empty!\n");
		return 1;
	}
	else
	{
		return 0;
	}
}

int enqueue(struct queue *queue_head, struct queue *x)
{
	int full_flag = 0;
	full_flag = queue_is_full(queue_head);
	if(full_flag == 1)
	{
		return 0;//入队失败
	}

	queueall[queue_head->tail] = *x;
	if(queue_head->tail == QUEUE_LEN)
	{
		queue_head->tail = 1;
	}
	else
	{
		queue_head->tail = queue_head->tail + 1;
	}
	
	return 1;
}

struct queue* dequeue(struct queue *queue_head)
{
	struct queue *deq_element = NULL;
	int empty_flag = 0;
	empty_flag = queue_is_empty(queue_head);
	if(empty_flag == 1)
	{
		return NULL;
	}
	
	deq_element = &queue_head[queue_head->head];
	if(queue_head->head == QUEUE_LEN)
		queue_head->head = 1;
	else
		queue_head->head = queue_head->head + 1;
	
	return deq_element;
}


int main(void)
{
	struct queue node = {0};
	int i;
	queue_init(&node);
	struct queue test[4] = {0};
	for(i = 0; i < 3; i++)
	{
		if(enqueue(&node, &test[i]) == 0)
		{
			return 0;
		}
	}
	for(i = 0; i < 3; i++)
	{
		if(dequeue(&node) == NULL)
		{
			return 0;
		}
	}
	
	printf("%d, %s\n", __LINE__, __func__);
	dequeue(&node);
	printf("%d, %s\n", __LINE__, __func__);
	
	return 0;
}