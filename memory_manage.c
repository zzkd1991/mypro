#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define num 18
#define pagesize (1 << 2)//4字节
#define allsize num * pagesize
#define blocksize 6
#define blocksum num / blocksize


struct node{
	int nodeflag; //= 0;
	int recunusedinx[num];// = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};
};



struct page {
	int size;
	int pageflag;
	char *address;
};

struct block{
	struct list;
	struct page;
	struct size;
	struct bloflag;
};
char *beginaddress = NULL;

struct page pageallarray[num];
struct node node[num] = {0};


void sepamem(void)
{
	int i = 0;
	int j = 0;
	beginaddress = (char *)malloc(allsize);
	if(beginaddress == NULL)
		return;
	for(i = 0; i < num; i++)
	{
		pageallarray[i].address = (char *)&beginaddress[i + pagesize];
		pageallarray[i].pageflag = 0;//未分配
		pageallarray[i].size = pagesize;
		memset(pageallarray[i].address, 0, pageallarray[i].size);
	}
	for(i = 0; i < num; i++)
	{
		node[i].nodeflag = 0;
		for(j = 0; j < num; j++)
		{
			node[i].recunusedinx[j] = 20;
		}
	}
}

int getunusedpagesum(struct page *headpage)
{
	int i;
	int cout = 0;
	for(i = 0; i < num; i++)
	{
		if(headpage[i].pageflag == 0)
			cout++;
	}
	return cout;
}

int findunusednode(struct node * headnode)
{
	int i = 0;
	for(i = 0; i < num; i++)
	{
		if(headnode[i].nodeflag == 0)
		{
			return i;
		}
	}
	return i;
}

int  getreqmem(int reqsize, struct page *headpage)
{
	int cout;
	int pagesum = 0;
	int nodeindex;
	int i, j;
	cout = getunusedpagesum(headpage);
	if(cout * pagesize < reqsize)
	{
		printf("not enough mem\n");
		return -1;
	}
	
	pagesum = reqsize / pagesize + 1;//需要的page数量
	printf("reqsize is %d, pagesum is %d, pagesize is %d\n", reqsize, pagesum, pagesize);
	//nodeindex++;//寻找未被使用的节点
	nodeindex = findunusednode(&node[0]);
	
	node[nodeindex].nodeflag = 1;//表示该节点被使用
	for(i = 0, j = 0; i < num; i++)
	{
		if(j >= pagesum)
		{
			break;
		}
		if(pageallarray[i].pageflag == 0)
		{
			node[nodeindex].recunusedinx[j] = i;
			j++;
		}
	}
	
	printf("nodeindex is %d, j is %d\n", nodeindex, j);
	for(i = 0; i < num; i++)
	{
		printf("node array is %d,", node[nodeindex].recunusedinx[i]);
	}
	return nodeindex;
}

int  memaccupy(int reqsize, char *buffer, struct page *headpage)
{
	int index = 0;
	int i;
	index = getreqmem(reqsize, headpage);
	if(index < 0)
	{
		printf("not enough space\n");
		return index;
	}
	
	printf("enough space\n");
	for(i = 0; i < num; i++)
	{
		if(node[index].recunusedinx[i] != 20)
		{
			memcpy(pageallarray[node[index].recunusedinx[i]].address, buffer, pagesize);
			buffer += pagesize;
			pageallarray[node[index].recunusedinx[i]].pageflag = 1;
		}
	}
	
	return index;
}

void verifycontent(int index)
{
	int i;
	printf("pageallarray[node[index].recunusedinx[i]].size is %d\n", pageallarray[node[index].recunusedinx[i]].size);
	for(i = 0; i < num; i++)
	{
		if(node[index].recunusedinx[i] != 20)
		{
			for(i = 0; i < pageallarray[node[index].recunusedinx[i]].size; i++)
			{
				printf("content is %c", pageallarray[node[index].recunusedinx[i]].address[i]);
			}
			printf("\n--------------\n");
			//printf("content is %s\n", pageallarray[node[index].recunusedinx[i]].address);
		}
	}
}

void freespemem(int index)
{
	int i= 0;
	node[index].nodeflag = 0;//表示该节点未被使用
	for(i = 0; i < num; i++)
	{
		if(node[index].recunusedinx[i] != 20)
		{
			memcpy(pageallarray[node[index].recunusedinx[i]].address, 0, pagesize);
			pageallarray[node[index].recunusedinx[i]].pageflag = 0;
			node[index].recunusedinx[i] = 20;
		}
	}
}

void meminit(void)
{
	sepamem();
}

int main(void)
{
	meminit();
	int nodeindex = 0;
	char buffer[30] = "hello world\n";
	nodeindex = memaccupy(sizeof(buffer), buffer, &pageallarray[0]);
	if(nodeindex >= 0)
	{
		verifycontent(nodeindex);
	}
	freespemem(nodeindex);
	
	//verifycontent(nodeindex);
	return 0;
}



