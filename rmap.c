
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