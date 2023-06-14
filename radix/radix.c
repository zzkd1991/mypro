#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include "rax.h"

static inline void raxStatckInit(raxStack *ts)
{
	ts->stack = ts->static_items;
	ts->items = 0;
	ts->maxitems = RAX_STACK_STATIC_ITEMS;
	ts->oom = 0;
}

#define raxNodeCurrentLength(n) ( \
		sizeof(raxNode) + (n)->size+ \
		raxPadding((n)->size)+ \
		((n)->iscompr ? sizeof(raxNode*) : sizeof(raxNode*)*(n)->size)+ \
		(((n)->iskey && !(n)->isnull)*sizeof(void*))) \
		
raxNode *raxNewNode(size_t children, int datafield) {
	size_t nodesize = sizeof(raxNode)+children+raxPadding(children)+
					  sizeof(raxNode*)*children;
		   if(datafield) nodesize += sizeof(void*);
		   raxNode *node = rax_malloc(nodesize);
		   if(node == NULL) return NULL;
		   node->iskey = 0;
		   node->isnull = 0;
		   node->iscompr = 0;
		   node->size = children;
		   return node;
}


rax *raxNew(void)
{
	rax *rax = rax_malloc(sizeof(*rax));
	if(rax == NULL) return NULL;
	rax->numele = 0;
	rax->numnodes = 1;
	rax->head = raxNewNode(0, 0);
	if(rax->head == NULL)
	{
		rax_free(rax);
		return NULL;
	}else {
		return rax;
	}
}


raxNode *raxReallocForData(raxNode *n, void *data)
{
	if(data == NULL) return n;
	size_t curlen = raxNodeCurrentLength(n);
	return rax_realloc(n, currlen + sizeof(void *));
}

void raxSetData(raxNode *n, void *data)
{
	n->iskey = 1;
	if(data != NULL)
	{
		n->isnull = 0;
		void **ndata = (void **)((char*)n + raxNodeCurrentLength(n)-sizeof(void*));
		memcpy(ndata, &data, sizeof(data));
	} else{
		n->isnull = 1;
	}
}

void *raxGetData(raxNode *n)
{
	if(n->isnull) return NULL;
	void **ndata = (void**)((char*)n+raxNodeCurrentLength(n)-sizeof(void*));
	void *data;
	memcpy(&data, ndata, sizeof(data));
	return data;
}

raxNode *raxAddChild







