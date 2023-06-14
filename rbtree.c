#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ngx_rbt_red(node)	((node)->color = 1)
#define ngx_rbt_black(node)	((node)->color = 0)
#define ngx_rbt_is_red(node)	((node)->color)
#define ngx_rbt_is_black(node)	(!ngx_rbt_is_red(node))
#define ngx_rbt_copy_color(n1, n2)	(n1->color = n2->color)
#define NGX_TIMER_INFINITE	(ngx_msec_t) -1
#define STACK_SIZE		100

#define ngx_rbtree_sentinel_init(node)	ngx_rbt_black(node)//初始化哨兵节点

#define ngx_rbtree_init(tree, s, i)                                           \
    ngx_rbtree_sentinel_init(s);                                              \
    (tree)->root = s;                                                         \
    (tree)->sentinel = s;                                                     \
    (tree)->insert = i

typedef unsigned int uintptr_t;
typedef int intptr_t;
typedef unsigned char u_char;
typedef uintptr_t ngx_rbtree_key_t;
typedef unsigned int ngx_msec_int_t;
typedef ngx_rbtree_key_t ngx_msec_t;
typedef struct ngx_rbtree_node_s ngx_rbtree_node_t;
typedef intptr_t ngx_int_t;
typedef ngx_int_t ngx_rbtree_key_int_t;
typedef uintptr_t       ngx_uint_t;


struct ngx_rbtree_node_s{
	ngx_rbtree_key_t key;
	ngx_rbtree_node_t *left;
	ngx_rbtree_node_t *right;
	ngx_rbtree_node_t *parent;
	u_char color;
	u_char data;
};

	
		
typedef void (*ngx_rbtree_insert_pt)(ngx_rbtree_node_t *root, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);



struct ngx_rbtree_s{
	ngx_rbtree_node_t *root;
	ngx_rbtree_node_t *sentinel;
	ngx_rbtree_insert_pt insert;
};

typedef struct ngx_rbtree_s ngx_rbtree_t;


ngx_rbtree_node_t *ngx_rbtree_min(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
	while(node->left != sentinel)
	{
		node = node->left;
	}
	
	return node;
}

ngx_rbtree_node_t *ngx_rbtree_max(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
	while(node->right != sentinel)
	{
		printf("-------node->key is %d\n", (int)(node->key));
		node = node->right;
	}
	printf("-------node->key is %d\n", (int)(node->key));
	return node;
}

void 
	ngx_rbtree_left_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
		ngx_rbtree_node_t *node)
	{
		ngx_rbtree_node_t  *temp;
	
		temp = node->right;
		node->right = temp->left;
	
		if (temp->left != sentinel) {
			temp->left->parent = node;
		}
	
		temp->parent = node->parent;
	
		if (node == *root) {
			*root = temp;
	
		} else if (node == node->parent->left) {
			node->parent->left = temp;
	
		} else {
			node->parent->right = temp;
		}
	
		temp->left = node;
		node->parent = temp;
	}

static void ngx_rbtree_right_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
    ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->right) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}


void
ngx_rbtree_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        p = (node->key < temp->key) ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


void ngx_rbtree_insert_timer_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
	ngx_rbtree_node_t **p;
	
	for( ;; )
	{
		p = ((ngx_rbtree_key_int_t) (node->key - temp->key) < 0) ? &temp->left : &temp->right;
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
	ngx_rbt_red(node);
}


void
ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  **root, *temp, *sentinel;

    /* a binary tree insert */

    root = &tree->root;
    sentinel = tree->sentinel;

    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        ngx_rbt_black(node);
        *root = node;

        return;
    }

    tree->insert(*root, node, sentinel);

    /* re-balance tree */

    while (node != *root && ngx_rbt_is_red(node->parent)) {

        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;

            if (ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    ngx_rbtree_left_rotate(root, sentinel, node);
                }

                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        } else {
			printf("%s, %d\n", __func__, __LINE__);
            temp = node->parent->parent->left;

            if (ngx_rbt_is_red(temp)) {
				printf("%s, %d\n", __func__, __LINE__);
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->left) {
					printf("%s, %d\n", __func__, __LINE__);	
                    node = node->parent;
                    ngx_rbtree_right_rotate(root, sentinel, node);
                }
				printf("%s, %d\n", __func__, __LINE__);
                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    ngx_rbt_black(*root);
}


void
ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_uint_t           red;
    ngx_rbtree_node_t  **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */

    root = &tree->root;
    sentinel = tree->sentinel;

    if (node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if (node->right == sentinel) {
        temp = node->left;
        subst = node;

    } else {
        subst = ngx_rbtree_min(node->right, sentinel);
        temp = subst->right;
    }

    if (subst == *root) {
        *root = temp;
        ngx_rbt_black(temp);

        /* DEBUG stuff */
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    red = ngx_rbt_is_red(subst);

    if (subst == subst->parent->left) {
        subst->parent->left = temp;

    } else {
        subst->parent->right = temp;
    }

    if (subst == node) {

        temp->parent = subst->parent;

    } else {

        if (subst->parent == node) {
            temp->parent = subst;

        } else {
            temp->parent = subst->parent;
        }

        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        ngx_rbt_copy_color(subst, node);

        if (node == *root) {
            *root = subst;

        } else {
            if (node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        if (subst->left != sentinel) {
            subst->left->parent = subst;
        }

        if (subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    if (red) {
        return;
    }

    /* a delete fixup */

    while (temp != *root && ngx_rbt_is_black(temp)) {

        if (temp == temp->parent->left) {
            w = temp->parent->right;

            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                if (ngx_rbt_is_black(w->right)) {
                    ngx_rbt_black(w->left);
                    ngx_rbt_red(w);
                    ngx_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->right);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }

        } else {
            w = temp->parent->left;

            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                if (ngx_rbt_is_black(w->left)) {
                    ngx_rbt_black(w->right);
                    ngx_rbt_red(w);
                    ngx_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->left);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    ngx_rbt_black(temp);
}


ngx_rbtree_t	ngx_event_timer_rbtree;
static ngx_rbtree_node_t ngx_event_timer_sentinel;


ngx_int_t ngx_event_timer_init(void)
{
	ngx_rbtree_init(&ngx_event_timer_rbtree, &ngx_event_timer_sentinel, ngx_rbtree_insert_timer_value);
	return 1;
}

int stack_size = 0;

/*struct stack {
	ngx_rbtree_node_t elem;
	//int top;
};*/

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

void stack_push(ngx_rbtree_node_t *head, ngx_rbtree_node_t x)
{
	int result = 1;
	if(stack_size == STACK_SIZE)
	{
		printf("overflow\n");
		return;
	}
	if(result == 0)
		return;
	stack_size = stack_size + 1;
	//head[stack_size] = x;
	memcpy(&head[stack_size], &x, sizeof(x));
}

ngx_rbtree_node_t *stack_pop(ngx_rbtree_node_t *head)
{
	if(stack_size == 0)
	{
		printf("underflow\n");
		return NULL;
	}
	stack_size = stack_size - 1;
	return &head[stack_size + 1];
}

ngx_rbtree_node_t stackall[STACK_SIZE] = {0};

void show_element(ngx_rbtree_t *tree)
{
	ngx_rbtree_node_t *p = NULL;
	p = tree->root;
	printf("root->key is %d, color is %s, %s, %d\n", (int)(p->key), (ngx_rbt_is_red(p) ? "red" : "black"), __func__, __LINE__);	

	if(p->left != ngx_event_timer_rbtree.sentinel)
	{
		p = tree->root->left;
		printf("root->left->key is %d, color is %s, %s, %d\n", (int)(p->key), (ngx_rbt_is_red(p) ? "red" : "black"), __func__, __LINE__); 
		p = tree->root;
	}

	if(p->right != ngx_event_timer_rbtree.sentinel)
	{
		p = tree->root->right;
		printf("root->right->key is %d, color is %s, %s, %d\n", (int)(p->key), (ngx_rbt_is_red(p) ? "red" : "black"), __func__, __LINE__); 
		p = tree->root;
	}

	if(p->right->right != ngx_event_timer_rbtree.sentinel)
	{
		p = tree->root->right->right;
		printf("root->key is %d, color is %s, %s, %d\n", (int)(p->key), (ngx_rbt_is_red(p) ? "red" : "black"), __func__, __LINE__); 
		p = tree->root;
	}

	if((p->left->left == ngx_event_timer_rbtree.sentinel) && (p->left->right == ngx_event_timer_rbtree.sentinel) \
		&& (p->right->left == ngx_event_timer_rbtree.sentinel))
	{
		printf("it is right\n");
	}
}




void NRPreOrder(ngx_rbtree_t *tree)
{
	ngx_rbtree_node_t *p;
	ngx_rbtree_node_t temp;
	//printf("tree->root->left is %p, key is %d, tree->root->right is %p, key is %d\n", tree->root->left, (int)tree->root->left->key, tree->root->right, (int)tree->root->right->key);
	//printf("tree->root->left->left is %p, key is %d, tree->root->left->right is %p, key is %d\n", tree->root->left->left, (int)tree->root->left->left->key, tree->root->left->right, (int)tree->root->left->right->key);
	//printf("tree->root->right->left is %p, key is %d, tree->root->right->right is %p, key is %d\n", tree->root->right->left, (int)tree->root->right->left->key, tree->root->right->right, (int)tree->root->right->right->key);


	if(tree->root != ngx_event_timer_rbtree.sentinel)
	{
		//printf("tree->root is %d\n", (int)(tree->root->key));
		stack_push(stackall, *(tree->root));
		while(!stack_empty())
		{
			p = stack_pop(stackall);
			//printf("pop p->data is %d, stack_size is %d, %p\n", (int)(p->key), stack_size, p);
			printf("pop p->data is %d, stack_size is %d\n", (int)(p->key), stack_size);
			//printf("p->right is %p, p->left is %p\n", p->right, p->left);
			if(p->right != ngx_event_timer_rbtree.sentinel)
			{	
				memcpy(&temp, p, sizeof(*p));
				if((int)p->right->key == 7)
				{
					p->right->key	= (ngx_rbtree_key_t)700;
				}
				count1++;
				stack_push(stackall, *(p->right));
				//printf("p->right is %d, stack_size is %d, %p, %p\n", (int)(p->right->key), stack_size, p->right, p->left);
				if(temp.left != ngx_event_timer_rbtree.sentinel)
				{
					stack_push(stackall, *(temp.left));
					//printf("temp->left is %d, stack_size is %d, %p\n", (int)(temp.left->key), stack_size, temp.left);
				}
			}
			else
			{
				if(p->left != ngx_event_timer_rbtree.sentinel)
				{
					stack_push(stackall, *(p->left));
					//printf("p->left is %d, stack_size is %d, %p\n", (int)(p->left->key), stack_size, p->left);
				}
			}
		}
	}
	
}

void print_element(ngx_rbtree_node_t *node)
{
	if(node != ngx_event_timer_rbtree.sentinel)
	{
		print_element(node->left);
		printf("node->key is %d\n", (int)(node->key));
		print_element(node->right);
	}
	/*while(node != ngx_event_timer_rbtree.sentinel)
	{
		printf("node->key is %d\n", (int)(node->key));
		node = node->right;
	}*/
}

ngx_msec_t
ngx_event_find_max_timer(void)
{
	ngx_msec_int_t timer;
	ngx_rbtree_node_t *node, *root, *sentinel;
	
	if(ngx_event_timer_rbtree.root == &ngx_event_timer_sentinel)
	{
		return NGX_TIMER_INFINITE;
	}
	
	root = ngx_event_timer_rbtree.root;
	sentinel = ngx_event_timer_rbtree.sentinel;
	
	node = ngx_rbtree_max(root, sentinel);
	
	timer = (ngx_msec_int_t)(node->key);
	
	return (ngx_msec_t) timer;

}


ngx_msec_t 
ngx_event_find_timer(void)
{
	ngx_msec_int_t timer;
	ngx_rbtree_node_t *node, *root, *sentinel;
	
	if(ngx_event_timer_rbtree.root == &ngx_event_timer_sentinel)
	{
		return NGX_TIMER_INFINITE;
	}
	
	root = ngx_event_timer_rbtree.root;
	sentinel = ngx_event_timer_rbtree.sentinel;
	
	node = ngx_rbtree_min(root, sentinel);
	
	timer = (ngx_msec_int_t)(node->key);
	
	return (ngx_msec_t) timer;
}


int main(void)
{
	int i;
	ngx_msec_t timer_value;
	ngx_event_timer_init();
	
	ngx_rbtree_node_t timer[7];
	for(i = 0; i < 4; i ++)
	{
		timer[i].key = (ngx_rbtree_key_t)(i + 100);
		ngx_rbtree_insert(&ngx_event_timer_rbtree, &timer[i]);
	}
	timer[4].key = 125;
	timer[5].key = 120;
	timer[6].key = 122;
	ngx_rbtree_insert(&ngx_event_timer_rbtree, &timer[4]);
	ngx_rbtree_insert(&ngx_event_timer_rbtree, &timer[5]);
	ngx_rbtree_insert(&ngx_event_timer_rbtree, &timer[6]);

	//show_element(&ngx_event_timer_rbtree);
	printf("------------------------------\n");
	NRPreOrder(&ngx_event_timer_rbtree);
	printf("-------------------------------\n");
	//print_element(ngx_event_timer_rbtree.root);
	//timer_value = ngx_event_find_timer();
	//printf("timer_value is %d\n", (int)timer_value);
	
	//timer_value = ngx_event_find_max_timer();
	//printf("timer_valu
	return;
}
