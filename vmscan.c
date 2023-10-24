static unsigned long shrink_page_list(struct list_head *page_list,
						struct pglist_data *pgdat,
						struct scan_control *sc,
						enum ttu_flags ttu_flags,
						struct reclaim_stat *stat,
						bool ignore_reference)
{
	LIST_HEAD(ret_pages);
	LIST_HEAD(free_pages);
	unsigned nr_reclaimed = 0;
	unsigned pgactivate = 0;
	
	memset(stat, 0, sizeof(*stat));
	cond_resched();
	
	page = lru_to_page(page_list);
	list_del(&page->lru);
	
	if(!trylock_page(page))
		goto keep;
	
	VM_BUG_ON_PAGE(PageActive(page), page);
	
	nr_pages = compound_nr(page);
	
	sc->nr_scanned += nr_pages;
	
	if(unlikely(!page_evictable(page)))
		goto activate_locked;
	
	if(!sc->may_unmap && page_mapped(page))
		goto keep_locked;
	
	may_enter_fs = (sc->gfp_mask & __GFP_FS) ||
			(PageSwapCache(page) && (sc->gfp_mask & __GFP_IO));
			
			
	page_check_dirty_writeback(page, &dirty, &writeback);
	if(dirty || writeback)
		stat->nr_dirty++;
	
	if(dirty && !writeback)
		stat->nr_unqueued_dirty++;
	
	mapping = page_mapping(page);
if(((dirty || writeback) && mapping &&
		inode_write_congested(mapping->host)) ||
		(writeback && PageReclaim(page)))
		stat->nr_congested++;
		goto activate_locked;
	else if(sane_reclaim(sc) ||
				!PageReclaim(page) || !may_enter_fs)
	{
		unlock_page(page);
		wait_on_page_writeback(page);
		/* then go back and try same page again */
		list_add_tail(&page->lru, page_list);
	}
}