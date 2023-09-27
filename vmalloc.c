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

static __always_inline void
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

static __always_inline void
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

/**
