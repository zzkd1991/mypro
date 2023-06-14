#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct list{
	struct list *prev;
	struct list *next;
	int key;
};


struct L{
	struct list nil;
};

struct L l;

void sentinel_init(struct list *sentinel)
{
	sentinel->prev = sentinel;
	sentinel->next = sentinel;
}

void list_insert(struct list *sentinel, struct list *x)
{
	x->next = sentinel->next;
	sentinel->next->prev = x;
	sentinel->next = x;
	x->prev = sentinel;
}

void list_delete(struct list *x)
{
	x->prev->next = x->next;
	x->next->prev = x->prev;
}

struct list *list_search(struct list *sentinel, int key)
{
	struct list *x;
	x = sentinel->next;
	while((x != sentinel) && (x->key == key))
	{
		x = x->next;
	}
	return x;
}

void show_element(struct list *sentinel)
{
	struct list *x;
	x = sentinel->next;
	while(x != sentinel)
	{
		printf("x->key is %d\n", x->key);
		x = x->next;
	}
}

#define LIST_LENGTH	4

int main(void)
{
	int i;
	struct list a[LIST_LENGTH] = {0};
	sentinel_init(&l.nil);
	for(i = 0; i < LIST_LENGTH; i++)
	{
		a[i].key = (i + 100);
	}
	for(i = 0; i < LIST_LENGTH; i++)
	{
		list_insert(&l.nil, &a[i]);
	}
	show_element(&l.nil);
	printf("-----------------------\n");
	list_delete(&a[3]);
	show_element(&l.nil);
	printf("-----------------------\n");
	list_delete(&a[0]);
	show_element(&l.nil);
	printf("-----------------------\n");
	list_delete(&a[1]);
	show_element(&l.nil);
	printf("-----------------------\n");
	list_delete(&a[2]);
	show_element(&l.nil);
	printf("------------------------\n");
	list_delete(&a[1]);
	return 0;
}