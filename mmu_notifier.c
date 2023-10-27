

void __mmu_notifier_release(struct mm_struct *mm)
{
	struct mmu_notifier *mm;
	int id;
	
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(mn, &mm->mmu_notifier_mm->list, hlist)
		if(mn->ops->release)
			mn->ops->release(mn, mm);
		
	spin_lock(&mm->mmu_notifier_mm->lock);
	while(unlikely(!hlist_empty(&mm->mmu_notifier_mm->list)))
	{
		mn = hlist_entry(mm->mmu_notifier_mm->list.first,
					struct mmu_notifier,
					hlist);
					
		hlist_del_init_rcu(&mn->hlist);
	}
	spin_unlock(&mm->mmu_notifier_mm->lock);
	srcu_read_unlock(&srcu, id);
	
	synchronize_srcu(&srcu);
}

int __mmu_notifier_clear_flush_young(struct mm_struct *mm,
			unsigned long start,
			unsigned long end)
{
	struct mmu_notifier *mm;
	int young = 0, id;
	
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(mn, &mm->mmu_notifier_mm->list, hlist)
	{
		if(mn->ops->clear_flush_young)
			young |= mn->ops->clear_flush_young(mn, mm, start, end);
	}
	srcu_read_unlock(&srcu, id);
	
	return young;
}

int __mmu_notifier_clear_young(struct mm_struct *mm, unsigned long start, unsigned long end)
{
	struct mmu_notifier *mn;
	int young = 0, id;
	
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(mn, &mm->mmu_notifier_mm->list, hlist)
	{
		if(mn->ops->clear_young)
			young |= mn->ops->clear_young(mn, mm, start, end);
	}
	
	srcu_read_unlock(&srcu, id);
	
	return young;
}

int __mmu_notifier_test_young(struct mm_struct *mm, unsigned long address)
{
	struct mmu_notifier *mn;
	int young = 0, id;
	
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(mn, &mm->mmu_notifier_mm->list, hlist)
	{
		if(mn->ops->test_young)
		{
			young = mn->ops->test_young(mn, mm, address);
			if(young)
				break;
		}
	}
	
	srcu_read_unlock(&srcu, id);
	
	return young;
}

void __mmu_notifier_change_pte(struct mm_struct *mm, unsigned long address, pte_t pte)
{
	struct mmu_notifier *mn;
	int id;
	
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(mn, &mm->mmu_notifier_mm->list, hlist)
	{
		if(mn->ops->change_pte)
			mn->ops->change_pte(mn, mm, address, pte);
	}
	
	srcu_read_unlock(&srcu, id);
}

int __mmu_notifier_invalidate_range_start(struct mmu_notifier_range *range)
{
	struct mmu_notifier *mn;
	int ret = 0;
	int id;
	
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(mn, &range->mm->mmu_notifier_mm->list, hlist)
	{
		if(mn->ops->invalidate_range_start)
		{
			int _ret;
			
			if(!mmu_notifier_range_blockable(range))
				non_block_start();
			_ret = mn->ops->invalidate_range_start(mn, range);
			if(!mmu_notifier_range_blockable(range))
				non_block_end();
			if(_ret)
			{
				pr_info("%pS callback failed with %d in %sblockable context.\n",
					mn->ops->invalidate_range_start, _ret,
					!mmu_notifier_range_blockable(range) ? "non-" : "");
				WARN_ON(mmu_notifier_range_blockable(range) ||
					_ret != -EAGAIN);
				ret = _ret;
			}
		}
	}
	srcu_read_unlock(&srcu, id);
	
	return ret;
}