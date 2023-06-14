#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STACK_SIZE	10


struct stack {
	int top;
	char *p;
	int size;//p所指向的内存大小
};

int init_element(struct stack *x)
{
	x->p = malloc(x->size * sizeof(char));
	if(x->p == NULL)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

int stack_empty(struct stack *s)
{
	if(s->top == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void stack_push(struct stack *head, struct stack x)
{
	int result = 1;
	if(head->top == STACK_SIZE)
	{
		printf("overflow\n");
		return;
	}
	init_element(&x);
	if(result == 0)
		return;
	head->top = head->top + 1;
	head[head->top] = x;
}

struct stack* stack_pop(struct stack *head)
{
	if(head->top == 0)
	{
		printf("underflow\n");
		return NULL;
	}
	head->top = head->top - 1;
	return &head[head->top + 1];
}

struct stack stackall[STACK_SIZE] = {0};


int main(void)
{
	#define TEST_LEN (sizeof(stackall) / sizeof(stackall[0]))
	int i = 0;
	struct stack *p;
	
	
	struct stack test_stack[3] = {{0, NULL, 100}, {0, NULL, 20}, {0, NULL, 50}};
	for(i = 0; i < 3; i++)
	{
		stack_push(stackall, test_stack[i]);
	}
	
	for(i = 0; i < 3; i++)
	{
		p = stack_pop(stackall);
		printf("p->size is %d\n", p->size);
	}
	
	return 0;
}