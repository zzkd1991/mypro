/* Walk a vmap address to the struct page it maps */
struct page *vmalloc_to_page(const void *vmalloc_addr)
{
	unsigned long addr = (unsigned long) vmalloc_addr;
	struct page *page = NULL;
	pgd_t *pgd = pgd_offset_k(addr);
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;
	
	VIRTUAL_BUG_ON(!is_vmalloc_or_moudle_addr(vmalloc_addr));
	
	if(pgd_none(*pgd))
		return NULL;
	p4d = p4d_offset(pdg, addr);
	if(pd4_none(*p4d))
		return NULL;
	pud = pud_offset(p4d, addr);
	
	WARN_ON_ONCE(pud_bad(*pud));
	if(pud_none(*pud) || pud_bad(*pud))
		return NULL;
	pmd = pmd_offset(pud, addr);
	WARN_ON_ONCE(pmd_bad(*pmd));
	if(pmd_none(*pmd) || pmd_bad(*pmd))
		return NULL;
	
	ptep = pte_offset_map(pmd, addr);
	pte = *ptep;
	if(pte_present(pte))
		page = pte_page(pte);
	pte_unmap(ptep);
	return page;
}

/* Map a vmalloc()-space virtual address to the physical page frame number */
unsigned long vmalloc_to_pfn(const void *vmalloc_addr)
{
	return page_to_pfn(vmalloc_to_page(vmalloc_addr));
}

static __always_inline unsigned long
va_size(struct vmap_area *va)
{
	return (va->va_end - va->va_start);
}

static __always_inline unsigned long
get_subtree_max_size(struct rb_node *node)
{
	struct vmap_area *va;
	
	va = rb_entry_safe(node, struct vmap_area, rb_node);
	return va ? va->subtree_max_size : 0;
}

static __always_inline unsigned long
compute_subtree_max_size(struct vmap_area *va)
{
	return max3(va_size(va),
			get_subtree_max_size(va->rb_node.rb_left),
			get_subtree_max_size(va->rb_node.rb_right));
}

static atomic_long_t nr_vmalloc_pages;

unsigned long vmalloc_nr_pages(void)
{
	return atomic_long_read(&nr_vmalloc_pages);
}

static struct vmap_area *__find_vmap_area(unsigned long addr)
{
	struct rb_node *n = vmap_area_root.rb_node;
	
	while(n)
	{
		struct vmap_area *va;
		
		va = rb_entry(n, struct vmap_area, rb_node);
		if(addr < va->va_start)
			n = n->rb_left;
		else if(addr >= va->va_end)
			n = n->rb_right;
		else
			return va;
	}
}

static __always_inline struct rb_node **
find_va_links(struct vmap_area *va,
		struct rb_root *root, struct rb_node *from,
		struct rb_node **parent)
{
	struct vmap_area *tmp_va;
	struct rb_node **link;
	
	if(root)
	{
		link = &root->rb_node;
		if(unlikely(!*link))
		{
			*parent = NULL;
			return link;
		}
	}
	else
	{
		link = &from;
	}
	
	do
	{
		tmp_va = rb_entry(*link, struct vmap_area, rb_node);
		
		if(va->va_start < tmp_va->va_end &&
				va->va_end <= tmp_va->va_start)
			link = &(*link)->rb_left;
		else if(va->va_end > tmp_va->va_start &&
				va->va_start >= tmp_va->va_end)
			link = &(*link)->rb_right;
		else
			BUG();
	} while(*link);
	
	*parent = &tmp_va->rb_node;
	return link;
}

static void
link_va(struct vmap_area *va, struct rb_root *root,
			struct rb_node *parent, struct rb_node **link, struct list_head *head)
{
	if(likely(parent))
	{
		head = &rb_entry(parent, struct vmap_area, rb_node)->list;
		if(&parent->rb_right != link)
			head = head->prev;
	}
	
	/* Insert to the rb-tree */
	rb_link_node(&va->rb_node, parent, link);
	if(root = &free_vmap_area_root)
	{
		rb_insert_augmented(&va->rb_node,
			root, &free_vmap_area_rb_augment_cb);
		va->subtree_max_size = 0;
	}
	else
	{
		rb_insert_color(&va->rb_node, root);
	}
	
	/* Address-sort this list */
	list_add(&va->list, head);
}

static void
unlink_va(struct vmap_area *va, struct rb_root *root)
{
	if(WARN_ON(RB_EMPTY_NODE(&va->rb_node)))
		return;
	
	if(root == &free_vmap_area_root)
		rb_erase_augmented(&va->rb_node,
				root, &free_vmap_area_rb_augment_cb);
	else
		rb_erase(&va->rb_node, root);

	list_del(&va->list);
	RB_CLEAR_NODE(&va->rb_node);
}

static void
insert_vmap_area(struct vmap_area *va,
	struct rb_root *root, struct list_head *head)
{
	struct rb_node **link;
	struct rb_node *parent;
	
	link = find_va_links(va, root, NULL, &parent);
	link_va(va, root, parent, link, head);
}

static void
insert_vmap_area_augment(struct vmap_area *va,
	struct rb_node *from, struct rb_root *root,
	struct list_head *head)
{
	struct rb_node **link;
	struct rb_node *parent;
	
	if(from)
		link = find_va_links(va, NULL, from, &parent);
	else
		link = find_va_links(va, root, NULL, &parent);
	
	link_va(va, root, parent, link, head);
	augment_tree_propagate_from(va);
}

static __always_inline bool
is_within_this_va(struct vmap_area *va, unsigned long size,
		unsigned long align, unsigned long vstart)
{
	unsigned long nva_start_addr;
	
	if(va->va_start > vstart)
		nva_start_addr = ALIGN(va->va_start, align);
	else
		nva_start_addr = ALIGN(vstart, align);
	
	
	if(nva_start_addr + size < nva_start_addr ||
			nva_start_addr < vstart)
		return false;
	
	return (nva_start_addr + size <= va->va_end);
}

/* Find the first free block(lowest start address) in the tree,
* that will accomplish the request corresponding th passing
* parameters.
*/
static __always_inline struct vmap_area *
find_vmap_lowest_match(unsigned long size,
	unsigned long align, unsigned long vstart)
{
	struct vmap_area *va;
	struct rb_node *node;
	unsigned long lenght;
	
	/*Start from the root. */
	node = free_vmap_area_root.rb_node;
	
	/* Adjust the search size for alignment overhead. */
	length = size + align - 1;
	
	while(node)
	{
		va = rb_entry(node, struct vmap_area, rb_node);
		
		if(get_subtree_max_size(node->rb_left) >= length &&
				vstart < va->va_start)
		{
			node = node->rb_left;
		}
		else
		{
			if(is_within_this_va(va, size, align, vstart))
				return va;
			
			
			if(get_subtree_max_size(node->rb_right) >= length)
			{
				node = node->rb_right;
				continue;
			}
			
			while((node = rb_parent(node)))
			{
				va = rb_entry(node, struct vmap_area, rb_node);
				if(is_within_this_va(va, size, align, vstart))
					return va;
				
				
				if(get_subtree_max_size(node->rb_right) >= length &&
							vstart <= va->va_start)
				{
					node = node->rb_right;
					break;
				}
			}
		}
	}
	
	return NULL;
	
}

static struct vmap_area *
find_vmap_lowest_linear_match(unsigned long size,
	unsigned long align, unsigned long vstart)
{
	struct vmap_area *va;
	
	list_for_each_entry(va, &free_vmap_area_list, list)
	{
		if(!is_with_this_va(va, size, align, vstart))
			continue;
		
		return va;
	}
	
	return NULL;
}

static __always_inline enum fit_type
classify_va_fit_type(struct vmap_area *va,
		unsigned long nva_start_addr, unsigned long size)
{
	enum fit_type type;
	
	/* Check if it is within VA. */
	if(nva_start_addr < va->va_start ||
			nva_start_addr + size > va->va_end)
		return NOTHING_FIT;
		
	if(va->va_start == nva_start_addr)
	{
		if(va->va_end == nva_start_addr + size)
			type = FL_FIT_TYPE;
		else
			type = LE_FIT_TYPE;
	}
	else if(va->va_end == nva_start_addr + size)
	{
		type = RE_FIT_TYPE;
	}
	else
	{
		type = NE_FIT_TYPE;
	}
	
	return type;
}

static __always_inline int
adjust_va_to_fit_type(struct vmap_area *va,
	unsigned long nva_start_addr, unsigned long size,
	enum fit_type type)
{
	struct vmap_area *lva = NULL;
	
	if(type == FL_FIT_TYPE)
	{
		unlink_va(va, &free_vmap_area_root);
		kmem_cache_free(vmap_area_cachep, va);
	}
	else if(type == LE_FIT_TYPE)
	{
		va->va_start += size;
	}
	else if(type == RE_FIT_TYPE)
	{
		va->va_end = nva_start_addr;
	}
	else if(type == NE_FIT_TYPE)
	{
		lva = __this_cpu_xchg(ne_fit_preload_node, NULL);
		if(unlikely(!lva))
		{
			lva = kmem_cache_alloc(vmap_area_cachep, GFP_NOWAIT);
			if(!lva)
				return -1;
		}
		
		
		/*
		 * Build the remainder
		*/
		lva->va_start = va->va_start;
		lva->va_end = nva_start_addr;
		
		/*
		 * Shrink this VA to remaining size.
		*/
		va->va_start = nva_start_addr + size;
	}
	else
	{
		return -1;
	}
	
	
	if(type != FL_FIT_TYPE)
	{
		augment_tree_propagate_from(va);
		
		if(lva)
			insert_vmap_area_augment(lva, &va->rb_node,
				&free_vmap_area_root, &free_vmap_area_list);
	}
	
	return 0;
}

static __always_inline unsigned long
__alloc_vmap_area(unsigned long size, unsigned long align,
		unsigned long vstart, unsigned long end)
{
	unsigned long nva_start_addr;
	struct vmap_area *va;
	enum fit_type type;
	int ret;
	
	va = find_vmap_lowest_match(size, align, vstart);
	if(unlikely(!va))
		return vend;
	
	if(va->va_start > vstart)
		nva_start_addr = ALIGN(va->va_start, align);
	else
		nva_start_addr = ALIGN(vstart, align);
	
	/* Check the "vend" restriction. */
	if(nva_start_addr + size > vend)
		return vend;
	
	/* Classify what we have found. */
	type = classify_va_fit_type(va, nva_start_addr, size);
	if(WARN_ON_ONCE(type == NOTHING_FIT))
		return vend;
	
	/* Update the free vmap_area. */
	ret = adjust_va_to_fit_type(va, nva_start_addr, size, type);
	if(ret)
		return vend;
	
#if DEBUG_AUGMENT_LOWEST_MATCH_CHECK
	find_vmap_lowest_match_check(size);
#endif

	return nva_start_addr;
}

static struct vmap_area *alloc_vmap_area(unsigned long size,
				unsigned long align,
				unsigned long vstart, unsigned long vend,
				int node, gfp_t fpt_mask)
{
	struct vmap_area *va, *pva;
	unsigned long addr;
	int purged = 0;
	
	BUG_ON(!size);
	BUG_ON(offset_in_page(size));
	BUG_ON(!is_power_of_2(align));
	
	if(unlikely(!vmap_initialized))
		return ERR_PTR(-EBUSY);
	
	might_sleep();
	
	va = kmem_cache_alloc_node(vmap_area_cachep,
			gfp_mask & GFP_RECLAIM_MASK, node);
	if(unlikely(!va))
		return ERR_PTR(-ENOMEM);
	
	/*
	 * Only scan the relevant parts containing pointers to other objects
	 * to avoid false negatives.
	*/
	kmemleak_scan_area(&va->rb_node, SIZE_MAX, gfp_mask & GFP_RECLAIM_MASK);

retry:
	preempt_disable();
	if(!__this_cpu_read(ne_fit_preload_node))
	{
		preempt_enable();
		pva = kmem_cache_alloc_node(vmap_area_cachep, GFP_KERNEL, node);
		preempt_disable();
		
		if(__this_cpu_cmpxchg(ne_fit_preload_node, NULL, pva))
		{
			if(pva)
				kmem_cache_free(vmap_area_cachep, pva);
			
		}
	}
	
	spin_lock(&vmap_area_lock);
	preempt_enable();
	
	addr = __alloc_vmap_area(size, align, vstart, vend);
	if(unlikely(addr == vend))
		goto overflow;
	
	va->va_start = addr;
	va->va_end = addr + size;
	va->vm = NULL;
	insert_vmap_area(va, &vmap_area_root, &vmap_area_list);
	
	spin_unlock(&vmap_area_lock);
	
	BUG_ON(!IS_ALIGNED(va->va_start, align));
	BUG_ON(va->va_start < vstart);
	BUG_ON(va->va_end > vend);
	
	return va;
	
overflow:
	spin_unlock(&vmap_area_lock);
	if(!purged)
	{
		purge_vmap_area_lazy();
		purged = 1;
		goto retry;
	}
	
	
	if(gfpflags_allow_blocking(gfp_mask))
	{
		unsigned long freed = 0;
		blocking_notifier_call_chain(&vmap_notify_list, 0, &freed);
		if(freed > 0)
		{
			purged = 0;
			goto retry;
		}
	}
	
	if(!(gfp_mask & __GFP_NOWARN) && prink_ratelimit())
			pr_warn("vmap allocation for size %lu failed: use vmalloc=<size> to increase size\n",size);
	
	kmem_cache_free(vmap_area_cachep, va);
	return ERR_PTR(-EBUSY);
}

int register_vmap_purge_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vmap_notify_list, nb);
}

int unregister_vmap_purge_notifier(struct notifer_block *nb)
{
	return blocking_notifier_chain_unregister(&vmap_notify_list, nb);
}

static void __free_vmap_area(struct vmap_area *va)
{
	unlink_va(va, &vmap_area_root);
	
	merge_or_add_vmap_area(va, &free_vmap_area_root, &free_vmap_area_list);
}

static void free_vmap_area(struct vmap_area *va)
{
	spin_lock(&vmap_area_lock);
	__free_vmap_area(va);
	spin_unlock(&vmap_area_lock);
}

static void vunmap_page_range(unsigned long addr, unsigned long end)
{
	pgd_t *pgd;
	unsigned long next;
	
	BUG_ON(addr >= end);
	pgd = pgd_offset_k(addr);
	do
	{
		next = pgd_addr_end(addr, end);
		if(pgd_none_or_clear_bad(pgd))
			continue;
		vunmap_p4d_range(pgd, addr, next);
	}while(pgd++, addr = next, addr != end);
}

static int vmap_pte_range(pmd_t *pmd, unsigned long addr, 
			unsigned long end, pgprot_t prot, struct page **pages, int *nr)
{
	pte_t *pte;
	
	pte = pte_alloc_kernel(pmd, addr);
	if(!pte)
		return -ENOMEM;
	do
	{
		struct page *page = pages[*nr];
		
		if(WARN_ON(!pte_none(*pte)))
			return -EBUSY;
		if(WARN_ON(!page))
			return -ENOMEM;
		
		set_pet_at(&init_mm, addr, pte, mk_pte(page, prot));
		(*nr)++;
	}while(pte++, addr += PAGE_SIZE, addr != end);
	
	return 0;
}

static int vmap_pmd_range(pud_t *pud, unsigned long addr,
				unsigned long end, pgprot_t prot, struct page **pages, int *nr)
{
	pmd_t *pmd;
	unsigned long next;
	
	pmd = pmd_alloc(&init_mm, pud, addr);
	if(!pmd)
		return -ENOMEM;
	do
	{
		next = pmd_addr_end(addr, end);
		if(vmap_pte_range(pmd, addr, next, prot, pages, nr))
			return -ENOMEM;
	}while(pmd++, addr = next, addr != end);
	
	return 0;
}

static int vmap_pud_range(p4d_t *p4d, unsigned long addr,
					unsigned long end, pgprot_t prot, struct page **pages, int *nr)
{
	pud_t *pud;
	unsigned long next;
	
	pud = pud_alloc(&init_mm, p4d, addr);
	if(!pud)
		return -ENOMEM;
	do
	{
		next = pud_addr_end(addr, end);
		if(vmap_pmd_range(pud, addr, next, prot, pages, nr))
	}while(pud++, addr = next, addr != end);
	
	return 0;
}

static int vmap_p4d_range(pgd_t *pgd, unsigned long addr,
					unsigned long end, pgprot_t prot, struct page **pages, int *nr)
{
	p4d_t *p4d;
	unsigned long next;
	
	p4d = p4d_alloc(&init_mm, pgd, addr);
	if(!p4d)
		return -ENOMEM;
	
	do
	{
		next = p4d_addr_end(addr, end);
		if(vmap_pud_range(p4d, addr, next, prot, pages, nr))
			return -ENOMEM;
	}while(p4d++, addr = next, addr != end);
	
	return 0;
}

static int vmap_page_range_noflush(unsigned long start, unsigned long end,
							pgprot_t prot, struct page **pages)
{
	pgd_t *pgd;
	unsigned long next;
	unsigned long addr = start;
	int err = 0;
	int nr = 0;
	
	BUG_ON(addr >= end);
	pgd = pgd_offset_k(addr);
	do
	{
		next = pgd_addr_end(addr, end);
		err = vmap_p4d_range(pgd, addr, next, prot, pages, &nr);
		if(err)
			return err;
	}while(pgd++, addr = next, addr != end);
	
	return nr;
}

static int vmap_page_range(unsigned long start, unsigned long end,
				pgprot_t prot, struct page **pages)
{
	int ret;
	
	ret = vmap_page_range_noflush(start, end, prot, pages);
	flush_cache_vmap(start, end);
	return ret;
}

int is_vmalloc_or_module_addr(const void *x)
{

#if defined(CONFIG_MODULES) && defined(MODULES_VADDR)
	unsigned long addr = (unsigned long)x;
	if(addr >= MODULES_VADDR && addr < MODULES_END)
		return 1;
#endif

	return is_vmalloc_addr(x);
}

static bool __purge_vmap_area_lazy(unsigned long start, unsigned long end)
{
	unsigned long resched_threshold;
	struct llist_node *valist;
	struct vmap_area *va;
	struct vmap_area *n_va;
	
	lockdep_assert_held(&vmap_purge_lock);
	
	valist = llist_del_all(&vmap_purge_list);
	if(unlikely(valist == NULL))
		return false;
	
	
	vmalloc_sync_all();
	
	llist_for_each_entry(va, valist, purge_list)
	{
		if(va->va_start < start)
			start = va->va_start;
		if(va->va_end > end)
			end = va->va_end;
	}
	
	flush_tlb_kernel_range(start, end);
	resched_threshold = lazy_max_pages() << 1;
	
	spin_lock(&vmap_area_lock);
	llist_for_each_entry_safe(va, n_va, valist, purge_list)
	{
			unsigned long nr = (va->va_end - va->va_start) >> PAGE_SIZE;
			
			merge_or_add_vmap_area(va, &free_vmap_area_root, &free_vmap_area_list);
			
			atomic_long_sub(nr, &vmap_lazy_nr);
			
			if(atomic_long_read(&vmap_lazy_nr) < resched_threshold)
				cond_resched_lock(&vmap_area_lock);
	}
	
	spin_unlock(&vmap_area_lock);
	return true;
}


static void try_purge_vmap_area_lazy(void)
{
	if(mutex_trylock(&vmap_purge_lock))
	{
		__purge_vmap_area_lazy(ULONG_MAX, 0);
		mutex_unlock(&vmap_purge_lock);
	}
}

static void free_vmap_area_noflush(struct vmap_area *va)
{
	unsigned long nr_lazy;
	
	spin_lock(&vmap_area_lock);
	unlink_va(va, &vmap_area_root);
	spin_unlock(&vmap_area_lock);
	
	
	nr_lazy = atomic_long_add_return((va->va_end - va->va_start) >> PAGE_SHIFT, &vmap_lazy_nr);
	
	llist_add(&va->purge_list, &vmap_purge_list);
	
	if(unlikely(nr_lazy > lazy_max_pages()))
		try_purge_vmap_area_lazy();
}

static void free_unmap_vmap_area(struct vmap_area *va)
{
	flush_cache_vunmap(va->va_start, va->va_end);
	unmap_vmap_area(va);
	if(debug_pagealloc_enabled())
		flush_tlb_kernel_range(va->va_start, va->va_end);
	
	free_vmap_area_noflush(va);
}

static struct vmap_area *find_vmap_area(unsigned long addr)
{
	struct vmap_area *va;
	
	spin_lock(&vmap_area_lock);
	va = __find_vmap_area(addr);
	spin_unlock(&vmap_area_lock);
	
	return va;
}


struct vmap_block_queue {
	spinlock_t lock;
	struct list_head free;
};


struct vmap_block {
	spinlock_t lock;
	struct vmap_area *va;
	unsigned long free, dirty;
	unsigned long dirty_min, dirty_max;
	struct list_head free_list;
	struct rcu_head rcu_head;
	struct list_head purge;
};


static unsigned long addr_to_vb_idx(unsigned long addr)
{
	addr -= VMALLOC_START & ~(VMAP_BLOCK_SIZE-1);
	addr /= VMAP_BLOCK_SIZE;
	return addr;
}

static void *vmap_block_vaddr(unsigned long va_start, unsigned long pages_off)
{
	unsigned long addr;
	
	addr = va_start + (pages_off << PAGE_SHIFT);
	BUG_ON(addr_to_vb_idx(addr) != addr_to_vb_idx(va_start));
	return (void *)addr;
}

static void *new_vmap_block(unsigned int order, gfp_t gfp_mask)
{
	struct vmap_block_queue *vbq;
	struct vmap_block *vb;
	struct vmap_area *va;
	unsigned long vb_idx;
	int node, err;
	void *vaddr;
	
	node = numa_node_id();
	
	
	vb = kmalloc_node(sizeof(struct vmap_block), gfp_mask & GFP_RECLAIM_MASK, node);
	if(unlikely(!vb))
		return ERR_PTR(-ENOMEM);
	
	va = alloc_vmap_area(VMAP_BLOCK_SIZE, VMAP_BLOCK_SIZE, VMALLOC_START, VMALLOC_END, node, gfp_mask);
	
	if(IS_ERR(va))
	{
		kfree(vb);
		return ERR_CAST(va);
	}
	
	err = radix_tree_preload(gfp_mask);
	if(unlikely(err))
	{
		kfree(vb);
		free_vmap_area(va);
		return ERR_PTR(err);
	}
	
	
	vaddr = vmap_block_vaddr(va->va_start, 0);
	spin_lock_init(&vb->lock);
	vb->va = va;
	
	BUG_ON(VMAP_BBMAP_BITS <= (1UL << order));
	vb->free = VMAP_BBMAP_BITS - (1UL << order);
	vb->dirty = 0;
	vb->dirty_min = VMAP_BBMAP_BITS;
	vb->dirty_max = 0;
	INIT_LIST_HEAD(&vb->free_list);
	
	vb_idx = addr_to_vb_idx(va->va_start);
	spin_lock(&vmap_block_tree_lock);
	err = radix_tree_insert(&vmap_block_tree, vb_idx, vb);
	spin_unlock(&vmap_block_tree_lock);
	BUG_ON(err);
	radix_tree_preload_end();
	
	vbq = &get_cpu_var(vmap_block_queue);
	spin_lock(&vbq->lock);
	list_add_tail_rcu(&vb->free_list, &vbq->free);
	spin_unlock(&vbq->lock);
	put_cpu_var(vmap_block_queue);
	
	return vaddr;
}

static void free_vmap_block(struct vmap_block *vb)
{
	struct vmap_block *tmp;
	unsigned long vb_idx;
	
	vb_idx = addr_to_vb_idx(vb->va->va_start);
	spin_lock(&vmap_block_tree_lock);
	tmp = radix_tree_delete(&vmap_block_tree, vb_idx);
	spin_unlock(&vmap_block_tree_lock);
	BUG_ON(tmp != vb);

	free_vmap_area_noflush(vb->va);
	kfree_rcu(vb, rcu_head);
}

static void purge_fragmented_blocks(int cpu)
{
	LIST_HEAD(purge);
	struct vmap_block *vb;
	struct vmap_block *n_vb;
	struct vmap_block_queue *vbq = &per_cpu(vmap_block_queue, cpu);
	
	
	rcu_read_lock();
	list_for_each_entry_rcu(vb, &vbq->free, free_list)
	{
		if(!(vb->free + vb->dirty == VMAP_BBMAP_BITS && vb->dirty != VMAP_BBMAP_BITS))
			continue;
		
		spin_lock(&vb->lock);
		if(vb->free + vb->dirty == VMAP_BBMAP_BITS && vb->dirty != VMAP_BBMAP_BITS)
		{
			vb->free = 0;
			vb->dirty = VMAP_BBMAP_BITS;
			vb->dirty_min = 0;
			vb->dirty_max = VMAP_BBMAP_BITS;
			spin_lock(&vbq->lock);
			list_del_rcu(&vb->free_list);
			spin_unlock(&vbq->lock);
			spin_unlock(&vb->lock);
			list_add_tail(&vb->purge, &purge);
		}
		else
			spin_unlock(&vb->lock);
	}
	
	rcu_read_unlock();
	
	list_for_each_entry_safe(vb, n_vb, &purge, purge)
	{
		list_del(&vb->purge);
		free_vmap_block(vb);
	}
}

static void purge_fragmented_blocks_allcpus(void)
{
	int cpu;
	
	for_each_possible_cpu(cpu)
		purge_fragmented_blocks(cpu);
}

static void *vb_alloc(unsigned long size, gfp_t gfp_mask)
{
	struct vmap_block_queue *vbq;
	struct vmap_block *vb;
	void *vaddr = NULL;
	unsigned int order;
	
	BUG_ON(offset_in_page(size));
	BUG_ON(size > PAGE_SIZE * VMAP_MAX_ALLOC);
	if(WARN_ON(size == 0))
	{
		return 0;
	}
	
	order = get_order(size);
	
	rcu_read_lock();
	vbq = &get_cpu_var(vmap_block_queue);
	list_for_each_entry_rcu(vb, &vbq->free, free_list)
	{
		unsigned long pages_off;
		
		spin_lock(&vb->lock);
		if(vb->free < (1UL << order))
		{
			spin_unlock(&vb->lock);
			continue;
		}
		
		pages_off = VMAP_BBMAP_BITS - vb->free;
		vaddr = vmap_block_vaddr(vb->va->va_start, pages_off);
		vb->free -= 1UL << order;
		if(vb->free == 0)
		{
			spin_lock(&vbq->lock);
			list_del_rcu(&vb->free_list);
			spin_unlock(&vbq->lock);
		}
		
		spin_unlock(&vb->lock);
		break;
	}
	
	put_cpu_var(vmap_block_queue);
	rcu_read_unlock();
	
	/* Allocate new block if nothing was found */
	if(!vaddr)
		vaddr = new_vmap_block(order, gfp_mask);
	
	return vaddr;
}

static void vb_free(const void *addr, unsigned long size)
{
	unsigned long offset;
	unsigned long vb_idx;
	unsigned int order;
	struct vmap_block *vb;
	
	
	BUG_ON(offset_in_page(size));
	BUG_ON(size > PAGE_SIZE * VMAP_MAX_ALLOC);
	
	flush_cache_vunmap((unsigned long)addr, (unsigned long)addr + size);
	
	order = get_order(size);
	
	offset = (unsigned long)addr & (VMAP_BLOCK_SIZE - 1);
	offset >>= PAGE_SHIFT;
	
	vb_idx = addr_to_vb_idx((unsigned long)addr);
	rcu_read_lock();
	vb = radix_tree_lookup(&vmap_block_tree, vb_idx);
	rcu_read_unlock();
	BUG_ON(!vb);
	
	
	vunmap_page_range((unsigned long)addr, (unsigned long)addr + size);
	
	if(debug_pagealloc_enabled())
		flush_tlb_kernel_range((unsigned long)addr,
						(unsigned long)addr + size);
						
	spin_lock(&vb->lock);
	
	/*Expand dirty range */
	vb->dirty_min = min(vb->dirty_min, offset);
	vb->dirty_max = max(vb->dirty_max, offset + (1UL << order));
	
	vb->dirty += 1UL << order;
	if(vb->dirty == VMAP_BBMAP_BITS)
	{
		BUG_ON(vb->free);
		spin_unlock(&vb->lock);
		free_vmap_block(vb);
	}
	else
		spin_unlock(&vb->lock);
}

static void _vm_unmap_aliases(unsigned long start, unsigned long end, int flush)
{
	int cpu;
	
	if(unlikely(!vmap_initialized))
		return;
	
	might_sleep();
	
	for_each_possible_cpu(cpu)
	{
		struct vmap_block_queue *vbq = &per_cpu(vmap_block_queue, cpu);
		struct vmap_block *vb;
		
		rcu_read_lock();
		list_for_each_entry_rcu(vb, &vbq->free, free_list)
		{
			spin_lock(&vb->lock);
			if(vb->dirty)
			{
				unsigned long va_start = vb->va->va_start;
				unsigned long s, e;
				
				s = va_start + (vb->dirty_min << PAGE_SHIFT);
				e = va_start + (vb->dirty_max << PAGE_SHIFT);
				
				start = min(s, start);
				end = max(e, end);
				
				flush = 1;
			}
			
			spin_unlock(&vb->lock);
		}
		rcu_read_unlock();
	}
	
	mutex_lock(&vmap_purge_lock);
	purge_fragmented_blocks_allcpus();
	if(!__purge_vmap_area_lazy(start, end) && flush)
		flush_tlb_kernel_range(start, end);
	mutex_unlock(&vmap_purge_lock);
}

void vm_unmap_aliases(void)
{
	unsigned long start = ULONG_MAX, end = 0;
	int flush = 0;
	
	_vm_unmap_aliases(start, end, flush);
}

void vm_unmap_ram(const void *mem, unsigned int count)
{
	unsigned long size = (unsigned long)count << PAGE_SHIFT;
	unsigned long addr = (unsigned long)mem;
	struct vmap_area *va;
	
	might_sleep();
	BUG_ON(!addr);
	BUG_ON(addr < VMALLOC_START);
	BUG_ON(addr > VMALLOC_END);
	BUG_ON(!PAGE_ALIGNED(addr));
	
	if(likely(count <= VMAP_MAX_ALLOC))
	{
		debug_check_no_locks_freed(mem, size);
		vb_free(mem, size);
		return;
	}
	
	va = find_vmap_area(addr);
	BUG_ON(!va);
	debug_check_no_locks_freed((void *)va->va_start,
						(va->va_end - va->va_start));

	free_unmap_vmap_area(va);
}

void *vm_map_ram(struct page **pages, unsigned int count, int node, pgprot_t prot)
{
	unsigned long size = (unsigned long)count << PAGE_SHIFT;
	unsigned long addr;
	void *mem;
	
	if(likely(count <= VMAP_MAX_ALLOC))
	{
		mem = vb_alloc(size, GFP_KERNEL);
		if(IS_ERR(mem))
			return NULL;
		addr = (unsigned long)mem;
	}
	else
	{
		struct vmap_area *va;
		va = alloc_vmap_area(size, PAGE_SIZE,
				VMALLOC_START, VMALLOC_END, node, GFP_KERNEL);
		if(IS_ERR(va))
			return NULL;
		
		addr = va->va_start;
		mem = (void *)addr;
	}
	
	if(vmap_page_range(addr, addr + size, prot, pages) < 0)
	{
		vm_unmap_ram(mem, count);
		return;
	}
	
	return mem;
}

void __init vm_area_add_early(struct vm_struct *vm)
{
	struct vm_struct *tmp, **p;
	
	BUG_ON(vmap_initialized);
	for(p = *vmlist; (tmp = *p) != NULL; p = &tmp->next)
	{
		if(tmp->addr >= vm->addr)
		{
			BUG_ON(tmp->addr < vm->addr + vm->size);
			break;
		}
		else
			BUG_ON(tmp->addr + tmp->size > vm->addr);
	}
	
	vm->next = *p;
	*p =vm;
}


void __init vm_area_register_early(struct vm_struct *vm, size_t align)
{
	static size_t vm_init_off __initdata;
	unsigned long addr;
	
	addr = ALIGN(VMALLOC_START + vm_init_off, align);
	vm_init_off = PFN_ALIGN(addr + vm->size) - VMALLOC_START;
	
	vm->addr = (void *)addr;
	
	vm_area_add_early(vm);
}

static void vmap_init_free_space(void)
{
	unsigned long vmap_start = 1;
	const unsigned long vmap_end = ULONG_MAX;
	struct vmap_area *busy, *free;
	
	list_for_each_entry(busy, &vmap_area_list, list)
	{
		if(busy->va_start - vmap_start > 0)
		{
			free = kmem_cache_zalloc(vmap_area_cachep, GFP_NOWAIT);
			if(!WARN_ON_ONCE(!free))
			{
				free->va_start = vmap_start;
				free->va_end = busy->va_start;
				
				insert_vmap_area_augment(free, NULL,
						&free_vmap_area_root,
								&free_vmap_area_list);
			}
		}
		
		vmap_start = busy->va_end;
		
	}
	
	if(vmap_end - vmap_start > 0)
	{
		free = kmem_cache_zalloc(vmap_area_cachep, GFP_NOWAIT);
		if(!WARN_ON_ONCE(!free))
		{
			free->va_start = vmap_start;
			free->va_end = vmap_end;
			
			insert_vmap_area_augment(free, NULL,
				&free_vmap_area_root,
				&free_vmap_area_list);
		}
	}
}

void __init vmalloc_init(void)
{
	struct vmap_area *va;
	struct vm_struct *tmp;
	int i;
	
	vmap_area_cachep = KMEM_CACHE(vmap_area, SLAB_PANIC);
	
	for_each_possible_cpu(i)
	{
		struct vmap_block_queue *vbq;
		struct vfree_deferred *p;
		
		vbq = &per_cpu(vmap_block_queue, i);
		spin_lock_init(&vbq->lock);
		INIT_LIST_HEAD(&vbp->free);
		p = &per_cpu(vfree_deferred, i);
		init_llist_head(&p->list);
		INIT_WORK(&p->wq, free_work);
	}
	
	/* Import existing vmlist entries */
	for(tmp = vmlist; tmp; tmp = tmp->next)
	{
		va = kmem_cache_zalloc(vmap_area_cachep, GFP_NOWAIT);
		if(WARN_ON_ONCE(!va))
			continue;
		
		va->va_start = (unsigned long)tmp->addr;
		va->va_end = va->va_start + tmp->size;
		va->vm = tmp;
		insert_vmap_area(va, &vmap_area_root, &vmap_area_list);
	}
	
	vmap_init_free_space()
	vmap_initialized = true;
}

int map_kernel_range_noflush(unsigned long addr, unsigned long size,
					pgprot_t prot, struct page **pages)
{
	return vmap_page_range_noflush(addr, addr + size, prot, pages);
}

void unmap_kernel_range_noflush(unsigned long addr, unsigned long size)
{
	vunmap_page_range(addr, addr + size);
}

void unmap_kernel_range(unsigned long addr, unsigned long size)
{
	unsigned long end = addr + size;
	
	flush_cache_vunmap(addr, end);
	vunmap_page_range(addr, end);
	flush_tlb_kernel_range(addr, end);
}

int map_vm_area(struct vm_struct *area, pgprot_t prot, struct page **pages)
{
	unsigned long addr = (unsigned long)area->addr;
	unsigned long end = addr + get_vm_area_size(area);
	int err;
	
	err = vmap_page_range(addr, end, prot, pages);
	
	return err > 0 ? 0 : err;
}

static void setup_vmalloc_vm(struct vm_struct *vm, struct vmap_area *va,
					unsigned long flags, const void *caller)
{
	spin_lock(&vmap_area_lock);
	vm->flags = flags;
	vm->addr = (void *)va->va_start;
	vm->size = va->va_end - va->va_start;
	vm->caller = caller;
	va->vm = v;
	spin_unlock(&vmap_area_lock);
}

static struct vm_struct *__get_vm_area_node(unsigned long size,
		unsigned long align, unsigned long flags, unsigned long start,
		unsigned long end, int node, gfp_t gfp_mask, const void *caller)
{
	struct vmap_area *va;
	struct vm_struct *area;
	
	BUG_ON(in_interrupt());
	size = PAGE_ALIGN(size);
	if(unlikely(!size))
		return NULL;
	
	if(flags & VM_IOREMAP)
		align = 1ul << clamp_t(int, get_count_order_long(size),
								PAGE_SHIFT, IOREMAP_MAX_ORDER);
								
	area = kzalloc_node(sizeof(*area), gfp_mask & GFP_RECLAIM_MASK, node);
	if(unlikely(!area))
		return NULL;
	
	if(!(flag & VM_NO_GUARD))
		size += PAGE_SIZE;
	
	va = alloc_vmap_area(size, align, start, end, node, gfp_mask);
	if(IS_ERR(va))
	{
		kfree(area);
		return NULL;
	}
	
	setup_vmalloc_vm(area, va, flags, caller);
	
	return area;
}

struct vm_struct *__get_vm_area(unsigned long size, unsigned long flags,
					unsigned long start, unsigned long end)
{
	return __get_vm_area_node(size, 1, flags, start, end, NUMA_NO_NODE,
					GFP_KERNEL, __buildtin_return_address(0));
}

struct vm_struct *__get_vm_area_caller(unsigned long size, unsigned long flags,
						unsigned long start, unsigned long end,
						const void *caller)
{
	return __get_vm_area_node(size, 1, flags, start, end, NUMA_NO_NODE,
						GFP_KERNEL, caller);
}

struct vm_struct *get_vm_area(unsigned long size, unsigned long flags)
{
	return __get_vm_area_node(size, 1, flags, VMALLOC_START, VMALLOC_END,
					NUMA_NO_NODE, GFP_KERNEL,
					__builtin_return_address(0));
}

struct vm_struct *get_vm_area_caller(unsigned long size, unsigned long flags,
							const void *caller)
{
	return __get_vm_area_node(size, 1, flags, VMALLOC_START, VMALLOC_END,
					NUMA_NO_NODE, GFP_KERNEL, caller);
}

struct vm_struct *find_vm_area(const void *addr)
{
	struct vmap_area *va;
	
	va = find_vmap_area((unsigned long)addr);
	if(!va)
		return NULL;
	
	return va->vm;
}

struct vm_struct *remove_vm_area(const void *addr)
{
	struct vmap_area *va;
	
	might_sleep();
	
	spin_lock(&vmap_area_lock);
	va = __find_vmap_area((unsigned long)addr);
	if(va && va->vm)
	{
		struct vm_struct *vm = va->vm;
		
		va->vm = NULL;
		spin_unlock(&vmap_area_lock);
		
		kasan_free_shadow(vm);
		free_unmap_vmap_area(va);
		
		return vm;
	}
	
	spin_unlock(&vmap_area_lock);
	return NULL;
}

static inline void set_area_direct_map(const struct vm_struct *area,
								int (*set_direct_map)(struct page *page))
{
	int i;
	
	for(i = 0; i < area->nr_pages; i++)
		if(page_address(area->pages[i]))
			set_direct_map(area->pages[i]);
}

static void vm_remove_mappings(struct vm_struct *area, int deallocate_pages)
{
	unsigned long start = ULONG_MAX, end = 0;
	int flush_reset = area->flags & VM_FLUSH_RESET_PERMS;
	int flush_dmap = 0;
	int i;
	
	remove_vm_area(area->addr);
	
	
	if(!flush_reset)
		return;
	
	if(!deallocate_pages)
	{
		vm_unmap_aliases();
		return;
	}
	
	for(i = 0; i < area->nr_pages; i++)
	{
		unsigned long addr = (unsigned long)page_address(area->pages[i]);
		if(addr)
		{
			start = min(addr, start);
			end = max(addr + PAGE_SIZE, end);
			flush_dmap = 1;
		}
	}
	
	set_area_direct_map(area, set_direct_map_invalid_noflush);
	_vm_unmap_aliases(start, end, flush_dmap);
	set_area_direct_map(area, set_direct_map_default_noflush);
}

static void __vunmap(const void *addr, int deallocate_pages)
{
	struct vm_struct *area;
	
	if(!addr)
		return;
	
	if(WARN(!PAGE_ALIGNED(addr), "Trying to vfree() bad address (%p)\n", addr));
		return;
		
	area = find_vm_area(addr);
	if(unlikely(!area))
	{
		WARN(1, KERN_ERR "Trying to vfree() nonexistent vm area (%p)\n", addr);
		return;
	}
	
	debug_check_no_locks_freed(area->addr, get_vm_area_size(area));
	debug_check_no_obj_freed(area->addr, get_vm_area_size(area));
	
	vm_remove_mappings(area, deallocate_pages);
	
	if(deallocate_pages)
	{
		int i;
		
		for(i = 0; i < area->nr_pages; i++)
		{
			struct page *page = area->pages[i];
			
			BUG_ON(!page);
			__free_pages(page, 0);
		}
		atomic_long_sub(area->nr_pages, &nr_vmalloc_pages);
		kvfree(area->pages);
	}
	
	kfree(area);
	return;
}

static inline void __vfree_deferred(const void *addr)
{
	struct vfree_deferred *p = raw_cpu_ptr(&vfree_deferred);
	
	if(llist_add((struct llist_node *)addr, &p->list))
		schedule_work(&p->wq);
}

void vfree_atomic(const void *addr)
{
	BUG_ON(in_nmi());
	
	kmemleak_free(addr);
	
	if(!addr)
		return;
	__vfree_deferred(addr);
}

static void __vfree(const void *addr)
{
	if(unlikely(in_interrupt()))
		__vfree_deferred(addr);
	else
		__vunmap(addr, 1);
}

void vfree(const void *addr)
{
	BUG_ON(in_nmi());
	
	kmemleak_free(addr);
	
	might_sleep_if(!in_interrupt());
	if(!addr)
		return;
	
	__vfree(addr);
}

void vunmap(const void *addr)
{
	BUG_ON(in_interrupt());
	might_sleep();
	if(addr)
		__vunmap(addr, 0);
}

void *vmap(struct page **pages, unsigned int count,
			unsigned long flags, pgprot_t prot)
{
	struct vm_struct *area;
	unsigned long size;
	
	might_sleep();
	
	if(count > totalram_pages())
		return;
	
	size = (unsigned long)count << PAGE_SHIFT;
	area = get_vm_area_caller(size, flags, __builtin_return_address(0));
	if(!area)
		return NULL;
	
	if(map_vm_area(area, prot, pages))
	{
		vunmap(area->addr);
		return NULL;
	}
	
	return area->addr;
}

static void *__vmalloc_area_node(struct vm_struct *area, gfp_t gfp_mask, pgprot_t prot, int node)
{
	struct page **pages;
	unsigned int nr_pages, array_size, i;
	const gfp_t nested_gfp = (gfp_mask & GFP_RECLAIM_MASK) | __GFP_ZERO;
	const gfp_t alloc_mask = gfp_mask | __GFP_NOWARN;
	const gfp_t highmem_mask = (gfp_mask & (GFP_DMA | GFP_DMA32)) ? 0 : __GFP_HIGHMEM;
	
	nr_pages = get_vm_area_size(area) >> PAGE_SHIFT;
	array_size = (nr_pages * sizeof(struct page *));
	
	if(array_size > PAGE_SIZE)
	{
		pages = __vmalloc_node(array_size, 1, nested_gfp|highmem_mask, PAGE_KERNEL, node, area->caller)		
	}
	else
	{
		pages = kmalloc_node(array_size, nested_gfp, node);
	}
	
	if(!pages)
	{
		remove_vm_area(area->addr);
		kfree(area);
		return NULL;
	}
	
	area->pages = pages;
	area->nr_pages = nr_pages;
	
	for(i = 0; i < area->nr_pages; i++)
	{
		struct page *pages;
		
		if(node == NUMA_NO_NODE)
			page = alloc_page(alloc_mask|highmem_mask);
		else
			page = alloc_pages_node(node, alloc_mask|highmem_mask, 0);
		
		if(unlikely(!page))
		{
			area->nr_pages = i;
			atomic_long_add(area->nr_pages, &nr_vmalloc_pages);
			goto fail;
		}
		area->pages[i] = page;
		if(gfpflags_allow_blocking(gfp_mask|highmem_mask))
			cond_resched();
	}
	
	atomic_long_add(area->nr_pages, &nr_vmalloc_pages);
	
	if(map_vm_area(area, prot, pages))
		goto fail;
	return area->addr;
	
	
fail:
	warn_alloc(gfp_mask, NULL, "vmalloc: allocation failure, allocated %ld of %ld bytes", (area->nr_pages * PAGE_SIZE), area->size);
	
	__vfree(area->addr);
	return NULL;
}

void *__vmalloc_node_range(unsigned long size, unsigned long align,
					unsigned long start, unsigned long end, gfp_t gfp_mask,
					pgprot_t prot, unsigned long vm_flags, int node,
					const void *caller)
{
	struct vm_struct *area;
	void *addr;
	unsigned long real_size = size;
	
	size = PAGE_ALIGN(size);
	if(!size || (size >> PAGE_SHIFT) > totalram_pages())
		goto fail;
	
	area = __get_vm_area_node(size, align, VM_ALLOC | VM_UNINITIALIZED | vm_flags, start, end,
				node, gfp_mask, caller);
	if(!area)
		goto fail;
	
	addr = __vmalloc_area_node(area, gfp_mask, prot, node);
	if(!addr)
		return NULL;
	
	clear_vm_uninitialized_flag(area);
	
	kmemleak_vmalloc(area, size, gfp_mask);
	
	return addr;
	
fail:
	warn_alloc(gfp_mask, NULL, "vmalloc: allocation failure: %lu bytes", real_size);
	
	return NULL;
}

static void *__vmalloc_node(unsigned long size, unsigned long align,
				gfp_t gfp_mask, pgprot_t prot,
				int node, const void *caller)
{
	return __vmalloc_node_range(size, align, VMALLOC_START, VMALLOC_END,
				gfp_mask, prot, 0, node, caller);
}

void *__vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot)
{
	return __vmalloc_node(size, 1, gfp_mask, prot, NUMA_NO_NODE,
				__builtin_return_address(0));
}

static inline void *__vmalloc_node_flags(unsigned long size,
				int node, gfp_t flags)
{
	return __vmalloc_node(size, 1, flags, PAGE_KERNEL,
				node, __builtin_return_address(0));
}

void *__vmalloc_node_flags_caller(unsigned long size, int node, gfp_t flags, void *caller)
{
	return __vmalloc_node(size, 1, flags, PAGE_KERNEL, node, caller);
}

void *vmalloc(unsigned long size)
{
	return __vmalloc_node_flags(size, NUMA_NO_NODE, GFP_KERNEL);
}

void *vzalloc(unsigned long size)
{
	return __vmalloc_node_flags(size, NUMA_NO_NODE, GFP_KERNEL | __GFP_ZERO);
}

void *vmalloc_user(unsigned long size)
{
	return __vmalloc_node_range(size, SHMLBA, VMALLOC_START, VMALLOC_END,
				GFP_KERNE | __GFP_ZERO, PAGE_KERNEL,
				VM_USERMAP, NUMA_NO_NODE,
				__builtin_return_address(0));
}

void *vmalloc_node(unsigned long size, int node)
{
	return __vmalloc_node(size, 1, GFP_KERNEL, PAGE_KERNEL,
				node, __builtin_return_address(0));
}

void *vmalloc_exec(unsigned long size)
{
	return __vmalloc_node_range(size, 1, VMALLOC_START, VMALLOC_END,
					GFP_KERNEL, PAGE_KERNEL_EXEC, VM_FLUSH_RESET_PERMS,
					NUMA_NO_NODE, __builtin_return_address(0));
}

void *vmalloc_32(unsigned long size)
{
	return __vmalloc_node(size, 1, GFP_VMALLOC32, PAGE_KERNEL,
					NUMA_NO_NODE, __builtin_return_address(0));
}

void *vmalloc_32_user(unsigned long size)
{
	return __vmalloc_node_range(size, SHMLBA, VMALLOC_START, VMALLOC_END,
				GPF_VMALLOC32 | __GFP_ZERO, PAGE_KERNEL,
				VM_USERMAP, NUMA_NO_NODE,
				__builtin_return_address(0));
}

static int aligned_vread(char *buf, char *addr, unsigned long count)
{
	struct page *p;
	int copied = 0;
	
	while(count)
	{
		unsigned long offset, length;
		
		offset = offset_in_page(addr);
		length = PAGE_SIZE - offset;
		if(length > count)
			length = count;
		p = vmalloc_to_page(addr);
		
		if(p)
		{
			void *map = kmap_atomic(p);
			memcpy(buf, map + offset, length);
			kunmap_atomic(map);
		}
		else
		{
			memset(buf, 0, length);
		}
		
		addr += length;
		buf += length;
		copied += length;
		count -= length;
	}
	
	return copied;
}

static int aligned_vwrite(char *buf, char *addr, unsigned long count)
{
	struct page *p;
	int copied = 0;
	
	while(count)
	{
		unsigned long offset, length;
		
		offset = offset_in_page(addr);
		length = PAGE_SIZE - offset;
		if(length > count)
			length = count;
		
		p = vmalloc_to_page(addr);
		
		if(p)
		{
			void *map = kmap_atomic(p);
			memcpy(map + offset, buf, length);
			kunmap_atomic(map);
		}
		
		addr += length;
		buf += length;
		copied += length;
		count -= length;
	}
	return copied;
}

long vread(char *buf, char *addr, unsigned long count)
{
	struct vmap_area *va;
	struct vm_struct *vm;
	char *vaddr, *buf_start = buf;
	unsigned long buflen = count;
	unsigned long n;
	
	if((unsigned long)addr + count < count)
		count = -(unsigned long)addr;
	
	spin_lock(&vmap_area_lock);
	list_for_each_entry(va, &vmap_area_list, list)
	{
		if(!count)
			break;
		
		if(!va->vm)
			continue;
		
		vm = va->vm;
		vaddr = (char *)vm->addr;
		if(addr >= vaddr + get_vm_area_size(vm))
			continue;
		while(addr < vaddr)
		{
			if(count == 0)
				goto finished;
			*buf = '\0';
			buf++;
			addr++;
			count--;
		}
		
		n = vaddr + get_vm_area_size(vm) - addr;
		if(n > count)
			n = count;
		if(!(vm->flags & VM_IOREMAP))
			aligned_vread(buf, addr, n);
		else
			memset(buf, 0, n);
		buf += n;
		addr += n;
		count -= n;
	}
finished:
	spin_unlock(&vmap_area_lock);
	
	if(buf == buf_start)
		return 0;
	if(buf != buf_start + buflen)
		memset(buf, 0, buflen - (buf - buf_start));
	
	return buflen;
}






/**
