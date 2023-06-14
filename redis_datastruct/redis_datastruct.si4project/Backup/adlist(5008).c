
#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
list *listCreate(void)
{
	struct list *list;

	if((list = zmalloc(sizeof(*list))) == NULL)
		return NULL;
	list->head = list->tail = NULL;
	list->len = 0;
	list->dup = NULL;
	list->free = NULL;
	list->match = NULL;
	return list;
}

/* Remove all the elements from the list without destroying the list itself. */
void listEmpty(list * list)
{
	unsigned long len;
	listNode *current, *next;

	current = list->head;
	len = list->len;
	while(len--)
	{
		next = current->next;
		if(list->free) list->free(current->value);
		zfree(current);
		current = next;
	}
	list->head = list->tail = NULL;
	list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
void listRelease(list *list)
{
	listEmpty(list);
	zfree(list);
}


/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
list *listAddNodeHead(list * list, void * value)
{
	listNode *node;

	if((node = zmalloc(sizeof(*node)) == NULL)
		return NULL;
	node->value = value;
	if(list->len == 0)
	{
		list->head = list->tail = node;
		node->prev = node->next = NULL;
	}
	else
	{
		node->prev = NULL;
		node->next = list->head;
		list->head->prev = node;
		list->head = node;
	}
	list->len++;
	return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
list *listAddNodeTail(list *list, void *value)
{
	listNode *node;

	if((node = zmalloc(sizeof(*node))) == NULL)
		return NULL;
	node->value = value;
	if(list->len == 0)
	{
		list->head = list->tail = node;
		node->prev = node->next = NULL;
	}
	else
	{
		node->prev = list->tail;
		node->next = NULL;
		list->tail->next = node;
		list->tail = node;
	}
	list->len++;
	return list;
}

list *listInsertNode(list * list, listNode * old_node, void * value, int after)
{
	listNode *node;

	if((node = zmalloc(sizeof(*node))) == NULL)
		return NULL;

	node->value = value;
	if(after)
	{
		node->prev = old_node;
		node->next = old_node->next;
		if(list->tail == old_node)
		{
			list->tail = node;
		}
	}
	else
	{
		node->next = old_node;
		node->prev = old_node->prev;
		if(list->head == old_node)
		{
			list->head = node;
		}
	}
	if(node->prev != NULL)
	{
		node->prev->next = node;
	}

	if(node->next != NULL)
	{
		node->next->prev = node;
	}
	list->len++;
	return list;
}



