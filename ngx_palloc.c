
#define ngx_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))


typedef struct {
	u_char *last;
	u_char *end;
	ngx_pool_t *next;
	ngx_uint_t failed;
}ngx_pool_data_t;



struct ngx_pool_s{
	ngx_pool_data_t d;
	size_t max;
	ngx_pool_t *current;
	ngx_chain_t *chain;
	ngx_pool_large_t *cleanup;
	ngx_log_t *log;
};

static void *ngx_palloc_block(ngx_pool_t *pool, size_t size)
{
	u_char *m;
	size_t psize;
	ngx_pool_t *p, *new;
	
	psize = (size_t) (pool->d.end - (u_char *)pool);
	
	m = ngx_memalign(NGX_POOL_ALIGNMENT, psize, pool->log);
	if(m == NULL)
	{
		return NULL;
	}
	
	new = (ngx_pool_t *) m;
	
	new->d.end = m + psize;
	new->d.next = NULL;
	new->d.failed = 0;
	
	m += sizeof(ngx_pool_data_t);
	m = ngx_align_ptr(m, NGX_ALIGNMENT);
	new->d.last = m + size;
	
	for(p = pool->current; p->d.next; p = p->d.next){
		if(p->d.failed++ > 4)
		{
			pool->current = p->d.next;
		}
	}
	
	p->d.next = new;
	
	return m;
}




void * ngx_palloc_small(ngx_pool_t *pool, size_t size, ngx_uint_t align)
{
	u_char *m;
	ngx_pool_t *p;
	
	p = pool->current;
	
	do {
		m = p->d.last;
		
		if(align){
			m = ngx_align_ptr(m, NGX_ALIGNMENT);
		}
		
		if((size_t) (p->d.end - m) >= size){
			p->d.last = m + size;
			
			return m;
		}
		
		p = p->d.next;
		
	}while(p);
	
	return ngx_palloc_block(pool, size);
}

static void *ngx_palloc_large(ngx_pool_t *pool, size_t size)
{
	void *p;
	ngx_uint_t n;
	ngx_pool_large_t *large;
	
	p = ngx_alloc(size, pool->log);
	if(p == NULL){
		return NULL;
	}
	
	n = 0;
	for(la)
	
	
	
}















