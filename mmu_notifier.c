

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

void __mmu_notifier_invalidate_range_end(struct mmu_notifier_range *range, bool only_end)
{
	struct mmu_notifer *mn;
	int id;
	
	lock_map_acquire(&__mmu_notifier_invalidate_range_start_map);
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(mn, &range->mm->mmu_notifier_mm->list, hlist)
	{
		if(!only_end && mn->ops->invalidate_range)
			mn->ops->invalidate_range(mn, range->mm,
								range->start,
								range->end);
		if(mn->ops->invalidate_range_end)
		{
			if(!mmu_notifier_range_blockable(range))
				non_block_start();
			mn->ops->invalidate_range_end(mn, range);
			if(!mmu_notifier_range_blockable(range))
				non_block_end();
		}
	}
	
	srcu_read_unlock(&srcu, id);
	lock_map_release(&__mmu_notifier_invalidate_range_start_map);
}

void __mmu_notifier_invalidate_range(struct mmu_struct *mm,
								unsigned long start, unsigned long end)
{
	struct mmu_notifier *mn;
	int id;
	
	id = srcu_read_lock(&srcu);
	hlist_for_each_entry_rcu(mn, &mm->mmu_notifier_mm->list, hlist)
	{
		if(mn->ops->invalidate_range)
			mn->ops->invalidate_range(mn, mm, start, end);
	}
	srcu_read_unlock(&srcu, id);
}

int __mmu_notifier_register(struct mmu_notifier *mn, struct mm_struct *mm)
{
	struct mmu_notifier_mm *mm_notifier_mm = NULL;
	int ret;
	
	lockep_assert_held_write(&mm->mmap_sem);
	BUG_ON(atomic_read(&mm->mm_users) <= 0);
	
	if(IS_ENABLED(CONFIG_LOCKDEP))
	{
		fs_reclaim_acquire(GFP_KERNEL);
		lock_map_acquire(&__mmu_notifier_invalidate_range_start_map);
		lock_map_release(&__mmu_notifier_invalidate_range_start_map);
		fs_reclaim_release(GFP_KERNEL);
	}
	
	mn->mm = mm;
	mn->users = 1;
	
	if(!mm->mmu_notifier_mm)
	{
		mmu_notifier_mm = kmalloc(sizeof(struct mmu_notifier_mm), GFP_KERNEL);
		if(!mmu_notifier_mm)
			return -ENOMEM;
		
		INIT_HLIST_HEAD(&mmu_notifier_mm->list);
		spin_lock_init(&mmu_notifier_mm->lock);
	}
	
	ret = mm_take_all_locks(mm);
	if(unlikely(ret))
		goto out_clean;
	
	mmgrab(mm);
	
	if(mmu_notifer_mm)
		mm->mmu_notifer_mm = mmu_notifer_mm;
	
	spin_lock(&mm->mmu_notifer_mm->lock);
	hlist_add_head_rcu(&mn->hlist, &mm->mmu_notifier_mm->list);
	spin_unlock(&mm->mmu_notifier_mm->lock);
	
	mm_drop_all_locks(mm);
	BUG_ON(atomic_read(&mm->mm_users) <= 0);
	return 0;
	
out_clean:
	kfree(mmu_notifer_mm);
	return ret;
}

int mmu_notifier_register(struct mmu_notifier *mn, struct mm_struct *mm)
{
	int ret;
	
	down_write(&mm->mmap_sem);
	ret = __mmu_notifier_register(mn, mm);
	up_write(&mm->mmap_sem);
	return ret;
}

static struct mmu_notifier *
find_get_mmu_notifier(struct mm_struct *mm, const struct mmu_notifier_ops *ops)
{
	struct mmu_notifer *mn;
	
	spin_lock(&mm->mmu_notifier_mm->lock);
	hlist_for_each_entry_rcu(mn, &mm->mmu_notifier_mm->list, hlist)
	{
		if(mn->ops != ops)
			continue;
		
		if(likely(mn->users != UINT_MAX))
			mn->users++;
		else
			mn = ERR_PTR(-EOVERFLOW);
		spin_unlock(&mm->mmu_notifier_mm->lock);
		return mn;
	}
	spin_unlock(&mm->mmu_notifier_mm->lock);
	return NULL;
}

struct mmu_notifier *mmu_notifier_get_locked(const struct mmu_notifier_ops *ops, struct mm_struct *mm)
{
	struct mmu_notifier *mn;
	int ret;
	
	lockdep_assert_held_write(&mm->mmap_sem);
	
	if(mm->mmu_notifier_mm)
	{
		mn = find_get_mmu_notifier(mm, ops);
		if(mn)
			return mn;
	}
	
	mn = ops->alloc_notifier(mm);
	if(IS_ERR(mn))
		return mn;
	mn->ops = ops;
	ret = __mmu_notifier_register(mn, mm);
	if(ret)
		goto out_free;
	return mn;
	
out_free:
	mn->ops->free_notifier(mn);
	return ERR_PTR(ret);
}

void __mmu_notifier_mm_destroy(struct mm_struct *mm)
{
	BUG_ON(!hlist_empty(&mm->mmu_notifier_mm->list));
	kfree(mm->mmu_notifier_mm);
	mm->mmu_notifier_mm = LIST_POISON1;
}

void mmu_notifier_unregister(struct mmu_notifier *mn, struct mm_struct *mm)
{
	BUG_ON(atomic_read(&mm->mm_count) <= 0);
	
	if(!hlist_unhashed(&mn->hlist))
	{
		int id;
		
		id = srcu_read_lock(&srcu);
		
		if(mn->ops->release)
			mn->ops->release(&srcu, id);
		srcu_read_unlock(&srcu, id);
		
		spin_lock(&mm->mmu_notifier_mm->lock);
		
		hlist_del_init_rcu(&mn->hlist);
		spin_unlock(&mm->mmu_notifier_mm->lock);
	}
	
	synchronize_srcu(&srcu);
	
	BUG_ON(atomic_read(&mm->mm_count <= 0));
	
	mmdrop(mm);
}

static void mmu_notifier_free_rcu(struct rcu_head *rcu)
{
	struct mmu_notifier *mn = container_of(rcu, struct mmu_notifier, rcu);
	struct mm_struct *mm = mn->mm;
	
	mn->ops->free_notifier(mn);
	mmdrop(mm);
}

void mmu_notifier_put(struct mmu_notifier *mn)
{
	struct mm_struct *mm = mn->mm;
	
	spin_lock(&mm->mmu_notifier_mm->lock);
	if(WARN_ON(!mn->users) || --mn->users)
		goto out_unlock;
	hlist_del_init_rcu(&mn->hlist);
	spin_unlock(&mm->mmu_notifier_mm->lock);
	
	call_srcu(&srcu, &mn->rcu, mmu_notifier_free_rcu);
	return;
	
out_unlock:
	spin_unlock(&mm->mmu_notifier_mm->lock);
}

void mmu_notifier_synchronize(void)
{
	synchronize_srcu(&srcu);
}

bool mmu_notifier_range_update_to_read_only(const struct mmu_notifier_range *range)
{
	if(!range->vma || range->event != MMU_NOTIFY_PROTECTION_VMA)
		return false;
	
	return range->vma->vm_flags & VM_READ;
}