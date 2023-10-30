
static struct kmem_cache *anon_vma_cachep;
static struct kmem_cache *anon_vma_chain_cachep;

static inline struct anon_vma *anon_vma_alloc(void)
{
	struct anon_vma *anon_vma;
	
	anon_vma = kmem_cache_alloc(anon_vma_cachep, GFP_KERNEL);
	if(anon_vma)
	{
		atomic_set(&anon_vma->refcount, 1);
		anon_vma->degree = 1;
		anon_vma->parent = anon_vma;
		
		anon_vam->root = anon_vma;
	}
	
	return anon_vma;
}

static inline void anon_vma_free(struct anon_vma *anon_vma)
{
	VM_BUG_ON(atomic_read(&anon_vma->refcount));
	
	might_sleep();
	if(rwsem_is_locked(&anon_vam->root->rwsem))
	{
		anon_vma_lock_write(anon_vma);
		anon_vma_unlock_write(anon_vma);
	}
	
	kmem_cache_free(anon_vma_cachep, anon_vma);
}

static inline struct anon_vma_chain *anon_vma_chain_alloc(gfp_t gfp)
{
	return kmem_cache_alloc(anon_vma_chain_cachep, gfp);
}

static void anon_vma_chain_free(struct anon_vma_chain *anon_vma_chain)
{
	kmem_cache_free(anon_vma_chain_cachep, anon_vma_chain);
}

static void anon_vma_chain_link(struct vm_area_struct *vma,
						struct anon_vma_chain *avc,
						struct anon_vma *anon_vma)
{
	avc->vma = vma;
	avc->anon_vma = anon_vma;
	list_add(&avc->same_vma, &vma->anon_vma_chain);
	anon_vma_interval_tree_insert(avc, &anon_vma->rb_root);
}

int __anon_vma_prepare(struct vm_area_struct *vma)
{
	struct mm_struct *mm = vma->vm_mm;
	struct anon_vma *anon_vma, *allocated;
	struct anon_vma_chain *avc;
	
	might_sleep();
	
	avc = anon_vma_chain_alloc(GFP_KERNEL);
	if(!avc)
		goto out_enomem;
	
	anon_vma = find_mergeable_anon_vma(vma);
	allocated = NULL;
	if(!anon_vma)
	{
		anon_vma = anon_vma_alloc();
		if(unlikely(!anon_vma))
			goto out_enomem_free_avc;
		allocated = anon_vma;
	}
	
	anon_vma_lock_write(anon_vma);
	
	spin_lock(&mm->page_table_lock);
	if(likely(!vma->anon_vma))
	{
		vma->anon_vma = anon_vma;
		anon_vma_chain_link(vma, avc, anon_vam);
		anon_vma->degree++;
		allocated = NULL;
		avc = NULL;
	}
	spin_unlock(&mm->page_table_lock);
	anon_vma_unlock_write(anon_vma);
	
	if(unlikely(allocated))
		put_anon_vma(allocated);
	if(unlikely(avc))
		anon_vma_chain_free(avc);
	
	return 0;
	
out_enomem_free_avc:
	anon_vma_chain_free(avc);
out_enomem:
	return -ENOMEM;
}

static inline struct anon_vma *lock_anon_vma_root(struct anon_vma *root, struct anon_vma *anon_vma)
{
	struct anon_vam *new_root = anon_vma->root;
	if(new_root != root)
	{
		if(WARN_ON_ONCE(root))
			up_write(&root->rwsem);
		root = new_root;
		down_write(&root->rwsem);
	}
	return root;
}

static inline void unlock_anon_vma_root(struct anon_vma *root)
{
	if(root)
		up_write(&root->rwsem);
}

int anon_vma_clone(struct vm_area_struct *dst, struct vma_area_struct *src)
{
	struct anon_vma_chain *avc, *pavc;
	struct anon_vma *root = NULL;
	
	list_for_each_entry_reverse(pavc, &src->anon_vma_chain, same_vma)
	{
		struct anon_vam *anon_vma;
		
		avc = anon_vma_chain_alloc(GFP_NOWAIT | __GFP_NOWARN);
		if(unlikely(!avc))
		{
			unlock_anon_vma_root(root);
			root = NULL;
			avc = anon_vma_chain_alloc(GFP_KERNEL);
			if(avc)
				goto enomem_failure;
		}
		anon_vma = pavc->anon_vma;
		root = lock_anon_vma_root(root, anon_vma);
		anon_vma_chain_link(dst, avc, anon_vma);
		
		if(!dst->anon_vam && anon_vma != src->anon_vma &&
			anon_vma->degree < 2)
			dst->anon_vma = anon_vma;
	}
	
	if(dst->anon_vma)
		dst->anon_vma->degree++;
	unlock_anon_vma_root(root);
	return 0;
	
enomem_failure:
	dst->anon_vma = NULL;
	unlink_anon_vmas(dst);
	return -ENOMEM;
}

int anon_vma_fork(struct vm_area_struct *vma, struct vm_area_struct *pvma)
{
	struct anon_vma_chain *avc;
	struct anon_vma *anon_vma;
	
	if(!pvma->anon_vma)
		return 0;
	
	/* Drop inherited anon_vma, we'll reuse existing or allocate new. */
	vma->anon_vma = NULL;
	
	
	error = anon_vma_clone(vma, pvma);
	if(error)
		return error;
	
	/* An existing anon_vma has been reused, all done then. */
	if(vma->anon_vma)
		return 0;
	
	/* Then add our own anon_vma. */
	anon_vma = anon_vma_alloc();
	if(!anon_vma)
		goto out_error;
	
	avc = anon_vma_chain_alloc(GFP_KERNEL);
	if(!avc)
		goto out_error_free_anon_vma;
	
	/* The root anon_vma's spinlock is the lock actually used when we
	* lock any of the anon_vmas in this anon_vma tree.
	*/
	anon_vma->root = pvma->anon_vma->root;
	anon_vma->parent = pvma->anon_vma;
	
	get_anon_vma(anon_vma->root);
	/* Mark this anon_vma as the one where our new (COWed) pages go. */
	vma->anon_vma = anon_vma;
	anon_vma_lock_write(anon_vma);
	anon_vma_chain_link(vma, avc, anon_vma);
	anon_vma->parent->degree++;
	anon_vma_unlock_write(anon_vma);
	
	return 0;

out_error_free_anon_vma:
	put_anon_vma(anon_vma);
out_error:
	unlink_anon_vmas(vma);
	return -ENOMEM;
}

void unlink_anon_vmas(struct vm_area_struct *vma)
{
	struct anon_vma_chain *avc, *next;
	struct anon_vma *root = NULL;
	
	/*
	 * Unlink each anon_vma chained to the VMA. This list is ordered
	 * from newest to oldest, ensuring the root anon_vma gets freed last.
	*/
	
	list_for_each_entry_safe(avc, next, &vma->anon_vma_chain, same_vma)
	{
		struct anon_vma *anon_vma = avc->anon_vma;
		
		root = lock_anon_vma_root(root, anon_vma);
		anon_vma_interval_tree_remove(avc, &anon_vma->rb_root);
		
		/*
		 * Leave empty anon_vmas on the list - we'll need
		 * to free them outside the lock.
		*/
		if(RB_EMPTY_ROOT(&anon_vma->rb_root.rb_root))
		{
			anon_vma->parent->degree--;
			continue;
		}
		
		list_del(&avc->same_vma);
		anon_vma_chain_free(avc);
	}
	
	if(vma->anon_vma)
		vma->anon_vma->degree--;
	unlock_anon_vma_root(root);
	
	
	list_for_each_entry_safe(avc, next, &vma->anon_vma_chain, same_vma)
	{
		struct anon_vma *anon_vma = avc->anon_vma;
		
		VM_WARN_ON(anon_vma->degree);
		put_anon_vma(anon_vma);
		
		list_del(&avc->same_vma);
		anon_vma_chain_free(avc);
	}
}

static void anon_vma_ctor(void *data)
{
	struct anon_vma *anon_vma = data;
	
	init_rwsem(&anon_vma->rwsem);
	atomic_set(&anon_vma->refcount, 0);
	anon_vma->rb_root = RB_ROOT_CACHED;
}

void __init anon_vma_init(void)
{
	anon_vma_cachep = kmem_cache_create("anon_vma", sizeof(struct anon_vma),
				0, SLAB_TYPEASFE_BY_RCU|SLAB_PANIC|SLAB_ACCOUNT,
				anon_vma_ctor);
	anon_vma_chain_cachep = KMEM_CACHE(anon_vma_chain,
				SLAB_PANIC|SLAB_ACCOUNT);
}

struct anon_vma *page_get_anon_vma(struct page *page)
{
	struct anon_vma *anon_vma = NULL;
	unsigned long anon_mapping;
	
	rcu_read_lock();
	anon_mapping = (unsigned long)READ_ONCE(page->mapping);
	if((anon_mapping & PAGE_MAPPING_FLAGS) != PAGE_MAPPING_ANON)
		goto out;
	if(!page_mapped(page))
		goto out;
	
	anon_vma = (struct anon_vma *)(anon_mapping - PAGE_MAPPING_ANON);
	if(!atomic_inc_not_zero(&anon_vma->refcount))
	{
		anon_vma = NULL;
		goto out;
	}
	
	if(!page_mapped(page))
	{
		rcu_read_unlock();
		put_anon_vma(anon_vma);
		return NULL;
	}
	
out:
	rcu_read_unlock();
	
	return anon_vma;
}

struct anon_vma *page_lock_anon_vma_read(struct page *page)
{
	struct anon_vma *anon_vma = NULL;
	struct anon_vma *root_anon_vma;
	unsigned long anon_mapping;
	
	rcu_read_lock();
	anon_mapping = (unsigned long)READ_ONCE(page->mapping);
	if((anon_mapping & PAGE_MAPPING_FLAGS) != PAGE_MAPPING_ANON)
		goto out;
	if(!page_mapping(page))
		goto out;
	
	anon_vma = (struct anon_vma *)(anon_mapping - PAGE_MAPPING_ANON);
	root_anon_vma = READ_ONCE(anon_vma->root);
	if(down_read_trylock(&root_anon_vma->rwsem))
	{
		if(!page_mapping(page))
		{
			up_read(&root_anon_vma->rwsem);
			anon_vma = NULL;
		}
		goto out;
	}
	
	/* trylock failed, we got to sleep */
	if(!atomic_inc_not_zero(&anon_vma->refcount))
	{
		anon_vma = NULL;
		goto out;
	}
	
	if(!page_mapped(page))
	{
		rcu_read_unlock();
		put_anon_vma(anon_vma);
		return NULL;
	}
	
	/* we pinned the anon_vma, its safe to sleep */
	rcu_read_unlock();
	anon_vma_lock_read(anon_vma);
	
	if(atomic_dec_and_test(&anon_vma->refcount))
	{
		anon_vma_unlock_read(anon_vma);
		__put_anon_vma(anon_vma);
		anon_vma = NULL;
	}
	
	return anon_vma;
	
out:
	ruc_read_lock();
	return anon_vma;
}

void page_unlock_anon_vma_read(struct anon_vma *anon_vma)
{
	anon_vma_unlock_read(anon_vma);
}

unsigned long page_address_in_vma(struct page *page, struct vm_area_struct *vma)
{
	unsigned long address;
	if(PageAnon(page))
	{
		struct anon_vma *page__anon_vma = page_anon_vma(page);
		if(!vma->anon_vma || !page__anon_vma ||
			vma->anon_vma->root != page__anon_vma->root)
			return -EFAULT;
	}
	else if(page->mapping)
	{
		if(!vma->vm_file || vma->vm_file->f_mapping != page->mapping)
			return -EFAULT;
	}
	else
		return -EFAULT;
	
	address = __vma_address(page, vma);
	if(unlikely(address < vma->vm_start) || address >= vma->vm_end)
		return -EFAULT;
	return address;
}

pmd_t *mm_find_pmd(struct mm_struct *mm, unsigned long address)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd = NULL;
	pmd_t pmde;
	
	pgd = pgd_offset(mm, address);
	if(!pgd_present(*pgd))
		goto out;
	
	p4d = p4d_offset(pgd, adress);
	if(!p4d_present(*p4d))
		goto out;
	
	pud = pud_offset(p4d, address);
	if(!pud_present(*pud))
		goto out;
	
	pmd = pmd_offset(pud, address);
	
	pmde = *pmd;
	barrier();
	if(!pmd_present(pmde) || pmd_trans_huge(pmde))
		pmd = NULL;

out:
	return pmd;
}

struct page_referenced_arg
{
	int mapcount;
	int referenced;
	unsigned long vm_flags;
	struct mem_cgroup *memcg;
};

static bool page_referenced_one(struct page *page, struct vm_area_struct *vma,
					unsigned long address, void *arg)
{
	struct page_referenced_arg *pra = arg;
	struct page_vma_mapped_walk pvmw = {
		.page = page,
		.vma = vma,
		.address = address,
	};
	int referenced = 0;
	
	while(page_vma_mapped_walk(&pvmw))
	{
		address = pvmw.address;
		
		if(vma->vm_flags & VM_LOCKED)
		{
			page_vma_mapped_walk_done(&pvmw);
			pra->vm_flags |= VM_LOCKED;
			return false;
		}
		
		if(pvmw.pte)
		{
			if(ptep_clear_flush_young_notify(vma, address, pvmw.pte))
			{
				if(likely(!(vma->vm_flags & VM_SEQ_READ)))
					referenced++;
			}
		}
		else if(IS_ENABLED(CONFIG_TRANSPARENT_HUGPAGE))
		{
			if(pmdp_clear_flush_young_notify(vma, address, pvmw.pmd))
				referenced++;
		}
		else
		{
			WARN_ON_ONCE(1);
		}
		
		pra->mapcount--;
	}
	
	if(referenced)
		clear_page_idle(page);
	if(test_and_clear_page_young(page))
		referenced++;
	
	if(referenced)
	{
		pra->referenced++;
		pra->vm_flags |= vma->vm_flags;
	}
	
	if(!pra->mapcount)
		return false;
	
	return true;
}

static bool invalid_page_referenced_vma(struct vma_area_struct *vma, void *arg)
{
	struct page_referenced_arg *pra = arg;
	struct mem_cgroup *memcg = pra->memcg;
	
	if(!mm_match_cgroup(vma->vm_mm, memcg))
		return true;
	
	return false;
}

int page_referenced(struct page *page,
				int is_loced,
				struct mem_cgroup *memcg,
				unsigned long *vm_flags)
{
	int we_locked = 0;
	struct page_referenced_arg pra = {
		.mapcount = total_mapcount(page),
		.memcg = memcg,
	};
	struct rmap_walk_countrol rwc = {
		.rmap_one = page_referenced_one,
		.arg = (void *)&pra,
		.anon_lock = page_lock_anon_vma_read,
	};
	
	*vm_flags = 0;
	if(!pra.mapcount)
		return 0;
	
	if(!page_rmapping(page))
		return 0;
	
	if(!is_locked && (!PageAnon(page) || PageKsm(page)))
	{
		we_locked = trylock_page(page);
		if(!we_locked)
			return 1;
	}
	
	
	/*
	 * If we are reclaiming on behalf of a cgroup, skip
	 * counting on behalf of references from different
	 * cgroups
	*/
	if(memcg)
	{
		rwc.invalid_vma = invalid_page_referenced_vma;
	}
	
	rmap_walk(page, &rwc);
	*vma_flags = pra.vm_flags;
	
	if(we_locked)
		unlock_page(page);
	
	return pra.referenced;
}

static bool page_mkclean_one(struct page *page, struct vm_area_struct *vma, unsigned long address, void *arg)
{
	struct page_vma_mapped_walk pvmw = {
		.page = page,
		.vma = vma,
		.address = address,
		.flags = PVMW_SYNC,
	};
	struct mmu_notifier_range range;
	int *cleaned = arg;
	
	mmu_notifier_range_init(&range, MMU_NOTIFY_PROTECTION_PAGE,
				0, vma, vma->vm_mm, address,
				min(vma->vm_end, address + page_size(page)));
	mmu_notifier_invalidate_range_start(&range);
	
	
	while(page_vma_mapped_walk(&pvmw))
	{
		int ret = 0;
		
		address = pvmw.address;
		if(pvmw.pte)
		{
			pte_t entry;
			pte_t *pte = pvmw.pte;
			
			if(!pte_dirty(*pte) && !pte_write(*pte))
				continue;
			
			flush_cache_page(vma, address, pte_pfn(*pte));
			entry = ptep_clear_flush(vma, address, pte);
			entry = pte_wrprotect(entry);
			entry = pte_mkclean(entry);
			set_pte_at(vma->vm_mm, address, pte, entry);
			ret = 1;
		}
		else
		{
#ifdef CONFIG_TRANSPARENT_HUGE_PAGECACHE
			pmd_t *pmd = pvmw.pmd;
			pmd_t entry;
			
			if(!pmd_dirty(*mpd) && !pmd_write(*pmd))
				continue;
			
			flush_cache_page(vma, address, page_to_pfn(page));
			entry = pmdp_invalidate(vma, address, pmd);
			entry = pmd_wrprotect(entry);
			entry = pmd_mkclean(entry);
			set_pmd_at(vma->vm_mm, address, pmd, entry);
			ret = 1;
#else
			WARN_ON_ONCE(1);
#endif
		}
		
		
		if(ret)
			(*cleaned)++;
	}
	
	mmu_notifier_invalidate_range_end(&range);
	
	return true;
}

static bool invalid_mkclean_vma(struct vm_area_struct *vma, void *arg)
{
	if(vma->vm_flags & VM_SHARED)
		return false;
	
	return true;
}

int page_mkclean(struct page *page)
{
	int cleaned = 0;
	struct address_space *mapping;
	struct rmap_walk_control rwc = {
		.arg = (void *)&cleaned,
		.rmap_one = page_mkclean_one,
		.invlaid_vma = invalid_mkclean_vma,
	};
	
	BUG_ON(!PageLocked(page));
	
	if(!page_mapped(page))
		return 0;
	
	mapping = page_mapping(page);
	if(!mapping)
		return 0;
	
	rmap_walk(page, &rwc);
	
	return cleaned;
}

/* move a page to our anon_vma */
void page_move_anon_rmap(struct page *page, struct vm_area_struct *vma)
{
	struct anon_vma *anon_vma = vma->anon_vma;
	
	page = compund_head(page);
	
	VM_BUG_ON_PAGE(!PageLoced(page), page);
	VM_BUG_ON_VMA(!anon_vma, vma);
	
	anon_vma = (void *) anon_vma + PAGE_MAPPING_ANON;
	
	WRITE_ONCE(page->mapping, (struct address_space *) anon_vma);
}

static void __page_set_anon_rmap(struct page *page,
			struct vm_area_struct *vma, unsigned long address, int exclusive)
{
	struct anon_vma *anon_vma = vma->anon_vma;
	
	BUG_ON(!anon_vma);
	
	if(PageAnon(page))
		return;
	
	
	if(!exclusive)
		anon_vma = anon_vma->root;
	
	anon_vma = (void *) anon_vma + PAGE_MAPPING_ANON;
	page->mapping = (struct address_space *) anon_vma;
	page->index = liner_page_index(vma, address);
}

void page_add_anon_rmap(struct page *page,
		struct vm_area_struct *vma, unsigned long address, bool compound)
{
	do_page_add_anon_rmap(page, vma, address, compound ? RMAP_COMPOUND : 0);
}

void do_page_add_anon_rmap(struct page *page,
			struct vm_area_struct *vma, unsigned long address, int flags)
{
	bool compound = flags & RMAP_COMPOUND;
	bool first;
	
	if(compound)
	{
		atomic_t *mapcount;
		VM_BUG_ON_PAGE(!PageLocked(page), page);
		VM_BUG_ON_PAGE(!PageTransHuge(page), page);
		mapcount = compound_mapcount_ptr(page);
		first = atomic_inc_and_test(mapcount);
	}
	else
	{
		first = atomic_inc_and_test(&page->_mapcount);
	}
	
	if(first)
	{
		int nr = compound ? hpage_nr_pages(page) : 1;
		
		if(compound)
			__inc_node_page_state(page, NR_ANON_THPS);
		__mod_node_page_state(page_pgdat(page), NR_ANON_MAPPED, nr);
	}
	
	if(unlikely(PageKsm(page)))
		return;
	
	VM_BUG_ON_PAGE(!PageLocked(page), page);
	
	if(first)
		__page_set_anon_rmap(page, vma, address,
				flag & RMAP_EXCLUSIVE);
	else
		__page_check_anon_rmap(page, vma, address);
}

void page_add_new_anon_rmap(struct page *page,
				struct vm_area_struct *vma, unsigned long address, bool compound)
{
	int nr = compound ? hpage_nr_pages(page) : 1;
	
	VM_BUG_ON_VMA(address < vma->vm_start || address >= vma->vm_end, vma);
	__SetPageSwapBacked(page);
	if(compound)
	{
		VM_BUG_ON_PAGE(!PageTransHuge(page), page);
		
		atomic_set(compound_mapcount_ptr(page), 0);
		__inc_node_page_state(page, NR_ANON_THPS);
	}
	else
	{
		VM_BUG_ON_PAGE(PageTransCompound(page), page);
		atomic_set(&page->_mapcount, 0);
	}
	
	__mode_node_page_state(page_pgdat(page), NR_ANON_MAPPED, nr);
	__page_set_anon_rmap(page, vma, address, 1);
}