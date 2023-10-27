
#define BITMAP_CHUNK_SIZE	sizeof(u64)
#define BITMAP_CHUNK_BITS	(BITMAP_CHUNK_SIZE * BITS_PER_BYTE)


static struct page *page_idle_get_page(unsigned long pfn)
{
	struct page *page;
	pg_data_t *pgdat;
	
	if(!pfn_valid(pfn))
		return NULL;
	
	
	page = pfn_to_page(pfn);
	if(!page || !PageLRU(page) ||
		!get_page_unless_zero(page))
		return NULL;
		
	pgdat = page_pgdat(page);
	spin_lock_irq(&pgdat->lru_lock);
	if(unlikely(!PageLRU(page)))
	{
		put_page(page);
		page = NULL;
	}
	spin_unlock_irq(&pgdat->lru_lock);
	return page;
}

static bool page_idle_clear_pte_refs_one(struct page *page,
					struct vm_area_struct *vma,
					unsigned long addr, void *arg)
{
	struct page_vma_mapped_walk pvmw = {
		.page = page,
		.vma = vma,
		.address = addr,
	};
	
	bool referenced = false;
	
	while(page_vma_mapped_walk(&pvmw))
	{
		addr = pvmw.address;
		if(pvmw.pte)
		{
			if(ptep_clear_young_notify(vma, addr, pvmw.pte))
				referenced = true;
		}
		else if(IS_ENABLED(CONFIG_TRANSPARENT_HUGEPAGE))
		{
			if(pmdp_clear_young_notify(vma, addr, pvmw.pmd))
				referenced = true;
		}
		else
		{
			WARN_ON_ONCE(1);
		}
	}
	
	if(referenced)
	{
		clear_page_idle(page);
		
		set_page_young(page);
	}
	
	return true;
}

static void page_idle_clear_pte_refs(struct page *page)
{
	
	static const struct rmap_walk_control rwc = {
		.rmap_one = page_idle_clear_pte_refs_one,
		.anon_lock = page_lock_anon_vma_read,
	};
	bool need_lock;
	
	if(!page_mapped(page) ||
		!page_rmapping(page))
		return;
	
	
	need_lock = !PageAnon(page) || PageKsm(page);
	if(need_lock && !trylock_page(page))
		return;
	
	rmap_walk(page, (struct rmap_walk_control *)&rwc);
	
	if(need_lock)
		unlock_page(page);
}

static ssize_t page_idle_bitmap_read(struct file *file, struct kobject *kobj,
								struct bin_attribute *attr, char *buf,
								loff_t pos, size_t count)
{
	u64 *out = (u64 *)buf;
	struct page *page;
	unsigned long pfn, end_pfn;
	int bit;
	
	if(pos % BITMAP_CHUNK_SIZE || count % BITMAP_CHUNK_SIZE)
		return -EINVAL;
	
	pfn = pos * BITS_PER_BYTE;
	if(pfn >= max_pfn)
		return 0;
	
	end_pfn = pfn + count * BITS_PER_BYTE;
	if(end_pfn > max_pfn)
		end_pfn = max_pfn;
	
	for(; pfn < end_pfn; pfn++)
	{
		bit = pfn % BITMAP_CHUNK_BITS;
		if(!bit)
			*out = 0ULL;
		page = page_idle_get_page(pfn);
		if(page)
		{
			if(page_is_idle(page))
			{
				page_idle_clear_pte_refs(page);
				if(page_is_idle(page))
					*out |= 1ULL << bit;
			}
			put_page(page);
		}
		
		if(bit == BITMAP_CHUNK_BITS - 1)
			out++;
		cond_resched();
	}
	
	return (char *)out - buf;
}

static ssize_t page_idle_bitmap_write(struct file *file, struct kobject *kobj,
						struct bin_attribute *attr, char *buf,
						loff_t pos, size_t count)
{
	const u64 *in = (u64 *)buf;
	struct page *page;
	unsigned long pfn, end_pfn;
	int bit;
	
	if(pos % BITMAP_CHUNK_SIZE || count % BITMAP_CHUNK_SIZE)
		return -EINVAL;
	
	pfn = pos * BITS_PER_BYTE;
	if(pfn >= max_pfn)
		return -ENXIO;
	
	end_pfn = pfn + count * BITS_PER_BYTE;
	if(end_pfn > max_pfn)
		end_pfn = max_pfn;
	
	for(; pfn < end_pfn; pfn++)
	{
		bit = pfn % BITMAP_CHUNK_BITS;
		if((*in >> bit) & 1)
		{
			page = page_idle_get_page(pfn);
			if(page)
			{
				page_idle_clear_pte_refs(page);
				set_page_idle(page);
				put_page(page);
			}
		}
		if(bit == BITMAP_CHUNK_BITS - 1)
			in++;
		cond_resched();
	}
	return (char *)in -buf;
}