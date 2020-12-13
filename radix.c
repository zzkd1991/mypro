#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ngx_radixtree_init(tree, s, i)                                           \
    (tree)->root = s;                                                         \
    (tree)->sentinel = s;                                                     \
    (tree)->insert = i


typedef struct ngx_radix_node_s ngx_radix_node_t;
typedef struct ngx_radixtree_s ngx_radixtree_t;
typedef int intptr_t;
typedef unsigned int uintptr_t;
typedef uintptr_t ngx_radix_key_t;
typedef uintptr_t       ngx_uint_t;
typedef intptr_t ngx_int_t;
typedef ngx_int_t ngx_radixtree_key_int_t;
typedef void (*ngx_radix_insert_pt)(ngx_radix_node_t *root, ngx_radix_node_t *node, ngx_radix_node_t *sentinel);


struct ngx_radix_node_s{
	ngx_radix_key_t key;
	ngx_radix_node_t *left;
	ngx_radix_node_t *right;
	ngx_radix_node_t *parent;
};

struct ngx_radixtree_s{
	ngx_radix_node_t *root;
	ngx_radix_node_t *sentinel;
	ngx_radix_insert_pt insert;
};

ngx_radixtree_t	ngx_event_timer_radixtree;
static ngx_radix_node_t ngx_event_timer_sentinel;

void ngx_radixtree_insert_timer_value(ngx_radix_node_t *temp, ngx_radix_node_t *node, ngx_radix_node_t *sentinel)
{
	ngx_radix_node_t **p;
	
	for( ;; )
	{
		p = ((ngx_radixtree_key_int_t) (node->key - temp->key) < 0) ? &temp->left : &temp->right;
		if(*p == sentinel)
		{
			break;
		}
		
		temp = *p;
	}
	
	*p = node;
	node->parent = temp;
	node->left = sentinel;
	node->right = sentinel;
}

void ngx_radixtree_insert(ngx_radixtree_t *tree, ngx_radix_node_t *node)
{
	ngx_radix_node_t **root, *temp, *sentinel;
	
	root = &tree->root;
	sentinel = tree->sentinel;
	
	if(*root == sentinel){
		node->parent = NULL;
		node->left = sentinel;
		node->right = sentinel;
		*root = node;
		
		return;
	}
	
	tree->insert(*root, node, sentinel);

}

ngx_radix_node_t *radix_tree_minimum(ngx_radix_node_t *node, ngx_radix_node_t *sentinel)
{
	while(node->left != sentinel)
	{
		node = node->left;
	}
	return node;
}

void ngx_radixtree_transplant(ngx_radixtree_t *tree, ngx_radix_node_t *u, ngx_radix_node_t *v)
{
	if(u->parent == NULL)
	{
		tree->root = v;
	}
	else if(u == u->parent->left)
	{
		u->parent->left = v;
	}
	else
	{
		u->parent->right = v;
	}
	if(v != tree->sentinel)
	{
		v->parent = u->parent;
	}
}

void ngx_radixtree_delete(ngx_radixtree_t *tree, ngx_radix_node_t *z)
{
	ngx_radix_node_t *x;
	ngx_radix_node_t *y;
	x = tree->root;
	if(z->left == tree->sentinel)
	{
		ngx_radixtree_transplant(tree, z, z->right);
	}
	else if(z->right == tree->sentinel)
	{
		ngx_radixtree_transplant(tree, z, z->left);
	}
	else
	{
		y = radix_tree_minimum(z->right, tree->sentinel);
		if(y->parent != z)
		{
			ngx_radixtree_transplant(tree,y, y->right);
			y->right = z->right;
			y->right->parent = y;
		}
		
		ngx_radixtree_transplant(tree, z, y);
		y->left = z->left;
		y->left->parent = y;
	}
}

//stack
#define STACK_SIZE   100
int stack_size = 0;
ngx_radix_node_t stackall[STACK_SIZE] = {0};

int stack_empty(void)
{
	if(stack_size == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void stack_push(ngx_radix_node_t *head, ngx_radix_node_t x)
{
	if(stack_size == STACK_SIZE)
	{
		printf("overflow\n");
		return;
	}
	stack_size = stack_size + 1;
	//head[stack_size] = x;
	memcpy(&head[stack_size], &x, sizeof(x));
}

ngx_radix_node_t *stack_pop(ngx_radix_node_t *head)
{
	if(stack_size == 0)
	{
		printf("underflow\n");
		return NULL;
	}
	stack_size = stack_size - 1;
	return &head[stack_size + 1];
}

void NRPreOrder(ngx_radixtree_t *tree)
{
	ngx_radix_node_t *p;
	ngx_radix_node_t temp;

	if(tree->root != ngx_event_timer_radixtree.sentinel)
	{
		//printf("tree->root is %d\n", (int)(tree->root->key));
		stack_push(stackall, *(tree->root));
		while(!stack_empty())
		{
			p = stack_pop(stackall);
			//printf("pop p->data is %d, stack_size is %d, %p\n", (int)(p->key), stack_size, p);
			printf("pop p->data is %d, stack_size is %d\n", (int)(p->key), stack_size);
			//printf("p->right is %p, p->left is %p\n", p->right, p->left);
			if(p->right != ngx_event_timer_radixtree.sentinel)
			{	
				memcpy(&temp, p, sizeof(*p));
				stack_push(stackall, *(p->right));
				//printf("p->right is %d, stack_size is %d, %p, %p\n", (int)(p->right->key), stack_size, p->right, p->left);
				if(temp.left != ngx_event_timer_radixtree.sentinel)
				{
					stack_push(stackall, *(temp.left));
				}
			}
			else
			{
				if(p->left != ngx_event_timer_radixtree.sentinel)
				{
					stack_push(stackall, *(p->left));
				}
			}
		}
	}
	
}

ngx_int_t ngx_event_timer_init(void)
{
	ngx_radixtree_init(&ngx_event_timer_radixtree, &ngx_event_timer_sentinel, ngx_radixtree_insert_timer_value);
	return 1;
}


int main(void)
{
	int i;
	ngx_radix_node_t timer[7];
	ngx_event_timer_init();
	for(i = 0; i < 4; i++)
	{
		timer[i].key = (ngx_radix_key_t)(i + 100);
		ngx_radixtree_insert(&ngx_event_timer_radixtree, &timer[i]);
	}
	timer[4].key = (ngx_radix_key_t)125;
	timer[5].key = (ngx_radix_key_t)120;
	timer[6].key = (ngx_radix_key_t)122;
	ngx_radixtree_insert(&ngx_event_timer_radixtree, &timer[4]);
	ngx_radixtree_insert(&ngx_event_timer_radixtree, &timer[5]);
	ngx_radixtree_insert(&ngx_event_timer_radixtree, &timer[6]);
	NRPreOrder(&ngx_event_timer_radixtree);
	printf("--------------------------------------\n");
	ngx_radixtree_delete(&ngx_event_timer_radixtree, &timer[4]);
	NRPreOrder(&ngx_event_timer_radixtree);
	return 0;
}

