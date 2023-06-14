

struct worker_pool {
	spinlock_t lock;
	int cpu;
	int node;
	int id;
	unsigned int flags;
	unsigned long watchdog_ts;
	struct list_head worklist;
	int nr_workers;
	int nr_idle;
	struct list_head idle_list;
	struct timer_list idle_timer;
	struct timer_list mayday_timer;
	
	DECLARE_HASHTABLE(busy_hash, BUSY_WORKER_HASH_ORDER);
	
	struct worker *manager;
	struct list_head workers;
	struct completion *detach_completion;
	struct ida	worker_ida;
	
	struct workqueue_attrs *attrs;
	struct hlist_node hash_node;
	int refcnt;
	
	atomic_t nr_running ____cacheline_aligned_in_smp;
	
	
	struct rcu_head rc;
} ____cacheline_aligned_in_smp;


struct pool_workqueue {
	struct worker_pool *pool;
	struct workqueue_struct *wq;
	int work_color;
	int flush_color;
	int refcnt;
	int nr_in_flight[WORK_NR_COLORS];
	
	int nr_active;
	int max_active;
	struct list_head delayed_works;
	struct list_head pwqs_node;
	struct list_head mayday_node;
	
	struct work_struct unbound_release_work;
	struct rcu_head rcu;
} __aligned(1 << WORK_STRUCT_FLAG_BITS);

/* Structure used to wait for workqueue flush */
struct wq_flusher {
	struct list_head list;	/* WQ: list of flushers */
	int	flush_color;		/* WQ: flush clor waiting for */
	struct completion done;	/* flush completion */
};

struct workqueue_struct {
	struct list_head pwqs;
	struct list_head list;
	struct mutex mutex;
	int work_color;
	int flush_color;
	atomic_t nr_pwqs_to_flush;
	struct wq_flusher *first_flusher;
	struct list_head flusher_queue;
	struct list_head flusher_overflow;
	
	struct list_head maydays;
	struct worker *rescuer;
	
	int nr_drainers;
	int saved_max_active;
	
	struct workqueue_attrs *unbound_attrs;
	struct pool_workqueue *dfl_pwq;
	
#ifdef CONFIG_SYSFS
	struct wq_device *wq_dev;
#endif
#ifdef CONFIG_LOCKDEP
	char *lock_name;
	struct lock_class_key key;
	struct lockdep_map lockdep_map;
#endif
	char name[WQ_NAME_LEN];
	
	struct rcu_head rcu;
	
	unsigned int flags ____cacheline_aligned;
	struct pool_workqueue __percpu *cpu_pwqs;
	struct pool_workqueue __rcu *numa_pwq_tbl[];
};

static struct kmem_cache *pwq_cache;

static cpumask_var_t *wq_numa_possible_cpumask;

static DEFINE_MUTEX(wq_pool_mutex);
static DEFINE_MUTEX(wq_pool_attach_mutex);
static DEFINE_SPINLOCK(wq_mayday_lock);
static DECLARE_WAIT_QUEUE_HEAD(wq_manager_wait);

static LIST_HEAD(workqueues);
static bool workqueue_freezing;

static LIST_HEAD(workqueues);
static bool workqueue_freezing;

struct workqueue_struct *system_wq __read_mostly;
struct workqueue_struct *system_highpri_wq;
struct workqueue_struct *system_long_wq;
struct workqueue_struct *system_unbound_wq;

/*
 * worker_pool_assign_id - allocate ID and assing it to @pool
 * @pool: the pool pointer of interest
 *
 * Returns 0 if ID in [0, WORK_OFFQ_POOL_NONE) is allocated and assigned
 * successfully, -errno on failure.
*/

static int worker_pool_assign_id(struct worker_pool *pool)
{
	int ret;
	
	lockep_assert_help(&wq_pool_mutex);
	
	ret = idr_alloc(&worker_pool_idr, pool, 0, WORK_OFFQ_POOL_NONE,
			GFP_KERNEL);
	if(ret >= 0)
	{
		pool->id = ret;
		return 0;
	}
	return ret;
}

static struct pool_workqueue *unbound_pwq_by_node(struct workqueue_struct *wq,
											int node)
{
	assert_rcu_or_mutex_or_pool_mutex(wq);
	
	if(unlikely(node == NUMA_NO_NODE))
		return wq->dfl_pwq;
	
	return rcu_dereference_raw(wq->numa_pwq_tbl[node]);
}

static unsigned int work_color_to_flags(int color)
{
	return color << WORK_STRUCT_COLOR_SHIFT;
}

static int get_work_color(struct work_struct *work)
{
	return (*work_databits(work) >> WORK_STRUCT_COLOR_SHIFT) &
		((1 << WORK_STRUCT_COLOR_BITS) -1)
}

static int work_next_color(int color)
{
	return (color + 1) % WORK_NR_COLORS;
}

static inline void set_work_data(struct work_struct *work, unsigned long data,
					unsigned long flags)
{
	WARN_ON_ONCE(!work_pending(work));
	atomic_long_set(&work_data, data | flags | work_static(work));
}

static void set_work_pwq(struct work_struct *work, struct pool_workqueue *pwq,
						unsigned long extra_flags)
{
	set_work_data(work, (unsigned long )pwq,
					WORK_STRUCT_PENDING | WORK_STRUCT_PWQ | extra_flags);
}

static void set_work_pool_and_keep_pending(struct work_struct *work,
						int pool_id)
{
	set_work_data(work, (unsigned long)pool_id << WORK_OFFQ_POOL_SHIFT,
					WORK_STRUCT_PENDING);
}

static void set_work_pool_and_clear_pending(struct work_struct *work,
					int pool_id)
{
	smp_wmb();
	set_work_data(work, (unsigned long)pool_id << WORK_OFFQ_POOL_SHIFT, 0);
	
	smp_mb();
}

static void clear_work_data(struct work_struct *work)
{
	smp_wmb();
	set_work_data(work, WORK_STRUCT_NO_POOL, 0);
}

static struct pool_workqueue *get_work_pwq(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);
	
	if(data & WORK_STRUCT_PWQ)
		return (void *)(data & WORK_STRUCT_WQ_DATA_MASK);
	else
		return NULL;
}

static struct worker_pool *get_work_pool(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);
	int pool_id;
	
	assert_ruc_or_pool_mutex();
	
	if(data & WORK_STRUCT_PWQ)
		return ((struct pool_workqueue *)
				(data & WORK_STRUCT_WQ_DATA_MASK))->pool;
				
	pool_id = data >> WORK_OFFQ_POOL_SHIFT;
	if(pool_id == WORK_OFFQ_POOL_NONE)
		return NULL;
	
	return idr_find(&worker_pool_idr, pool_id);
}

static int get_work_pool_id(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);
	
	if(data & WORK_STRUCT_PWQ)
		return ((struct pool_workqueue *)
					(data & WORK_STRUCT_WQ_DATA_MASK))->pool->id;
					
	return data >> WORK_OFFQ_POOL_SHIFT;				
}

static void mark_work_canceling(struct work_struct *work)
{
	unsigned long pool_id = get_work_pool_id(work);
	
	pool_id <<= WORK_OFFQ_POOL_SHIFT;
	set_work_data(work, pool_id | WORK_OFFQ_CANCELING, WORK_STRUCT_PENDING);
}

static bool work_is_canceling(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);
	
	return !(data & WORK_STRUCT_PWQ) && (data & WORK_OFFQ_CANCELING)
}

static bool __need_more_worker(struct worker_pool *pool)
{
	return !atomic_read(&pool->nr_running);
}

static bool need_more_worker(struct worker_pool *pool)
{
	return !list_empty(&pool->worklist) && __need_more_worker(pool);
}

static bool may_start_working(struct worker_pool *pool)
{
	return pool->nr_idle;
}

static bool keep_working(struct worker_pool *pool)
{
	return !list_empty(&pool->worklist) &&
		atomic_read(&pool->nr_running) <= 1;
}

static bool need_to_create_worker(struct worker_pool *pool)
{
	return need_more_worker(pool) && !may_start_working(pool);
}

static bool too_many_workers(struct worker_pool *pool)
{
	bool managing = pool->flags & POOL_MANAGER_ACTIVE;
	int nr_dile = pool->nr_dile + managing;
	int nr_busy = pool->nr_workers - nr_idle;
	
	return nr_idle > 2 && (nr_idle - 2) * MAX_IDLE_WORERS_RATIO >= nr_busy;
}

/*
 * Wake up funcitons.
*/

/* Return the first idle worker. Safe with preemption disabled */
static struct worker *first_idle_worker(struct worker_pool *pool)
{
	if(unlikely(list_empty(&pool->idle_list)))
		return NULL;
	
	return list_first_entry(&pool->idle_list, struct worker, entry);
}


static void wake_up_worker(struct worker_pool *pool)
{
	struct worker *worker = first_idle_worker(pool);
	
	if(likely(worker))
		wake_up_process(worker->task);
}

void wq_worker_running(struct task_struct *task)
{
	struct worker *worker = kthread_data(task);
	
	if(!worker->sleeping)
		return;
	if(!(worker->flags & WORKER_NOT_RUNNING))
		atomic_inc(&worker->pool->nr_running);
	worker->sleeping = 0;
}

void wq_worker_sleeping(struct task_struct *task)
{
	struct worker *next, *worker = kthread_data(task);
	struct worker_pool *pool;
	
	if(worker->flags & WORKER_NOT_RUNNING)
		return;
	
	pool = worker->pool;
	
	if(WARN_ON_ONCE(worker->sleeping))
		return;
	
	worker->sleeping = 1;
	spin_lock_irq(&pool->lock);
	
	if(atomic_dec_and_test(&pool->nr_running) &&
		!list_empty(&pool->worklist))
	{
		next = first_idle_worker(pool);
		if(next)
			wake_up_process(next->task);
	}
	spin_unlock_irq(&pool->lock);
}


work_func_t wq_worker_last_func(struct task_struct *task)
{
	struct worker *worker = kthread_data(task);
	
	return worker->last_func;
}

static inline void worker_set_flags(struct worker *worker, unsigned int flags)
{
	struct worker_pool *pool = worker->pool;
	
	WARN_ON_ONCE(worker->task != current);
	
	/* If transitioning into NOT_RUNNING, adjust nr_running. */
	if((flags & WORKER_NOT_RUNNING) &&
		!(worker->flags & WORKER_NOT_RUNNING))
	{
		atomic_dec(&pool->nr_running);
	}

	worker->flags |= flags;
}

static inline void worker_clr_flags(struct worker *worker, unsigned int flags)
{
	struct worker_pool *pool = worker->pool;
	unsigned int oflags = worker->flags;
	
	WARN_ON_ONCE(worker->task != current);
	
	worker->flags &= ~flags;
	
	/*
	 * If transitioning out of NOT_RUNNING, increment nr_running. Note
	 * that the nested NOT_RUNNING is not a noop. NOT_RUNNING is mask
	 * of multiple flags, not a single flag.
	*/
	if((flag & WORKER_NOT_RUNNING) && (oflags & WORKER_NOT_RUNNING))
		if(!(worker->flags & WORKER_NOT_RUNNING))
			atomic_inc(&pool->nr_running);
}

static struct worker *find_worker_executing_work(struct worker_pool *pool,
					struct work_struct *work)
{
	struct worker *worker;
	
	hash_for_each_possible(pool->busy_hash,  worker, hentry, (unsigned long)work)
		if(worker->current_work == work &&
			worker->current_func == work->func)
			return worker;
	
	return NULL;
}				

static void move_linked_works(struct work_struct *work, struct list_head *head,
					struct work_struct **nextp)
{
	struct work_struct *n;
	
	/*
	 * Linked worklist will always end before the end of the list,
	 * use NULL for list head.
	*/
	list_for_each_entry_safe_from(work, n, NULL, entry)
	{
		list_move_tail(&work->entry, head);
		if(!(*work_data_bits(work) & WORK_STRUCT_LINKED))
			break;
	}
	
	/*
	 * If we're alreay inside safe list traversal and have moved
	 * multiple works to the scheduled queue, the next position
	 * needs to be updated.
	*/
	
	if(nextp)
		*nextp = n;
}

static void get_pwq(struct pool_workqueue *pwq)
{
	lockdep_assert_held(&pwq->pool->lock);
	WARN_ON_ONCE(pwq->refcnt <= 0);
	pwq->refcnt++;
}

static void put_pwq(struct pool_workqueue *pwq)
{
	lockdep_assert_held(&pwq->pool->lock);
	if(likely(--pwq->refcnt))
		return;
	if(WARN_ON_ONCE(!(pwq->wq->flags & WQ_UNBOUND)))
		return;
	
	schedule_work(&pwq->unbound_release_work);
}

/*
 * put_pwq_unlocked - put_pwq() with surrounding pool lock/unlock
 * @pwq: pool_workqueue to put (can be %NULL)
 *
 * put_pwq() with locking. This function also allows %NULL @pwq.
*/
static void put_pwq_unlocked(struct pool_workqueue *pwq)
{
	if(pwq)
	{
		/*
		 * As both pwqs and pools are RCU protected, the
		 * following lock operations are safe.
		*/
		spin_lock_irq(&pwq->pool->lock);
		put_pwq(pwq);
		spin_unlock_irq(&pwq->pool->lock);
	}
}

static void pwq_activate_delayed_work(struct work_struct *work)
{
	struct pool_workqueue *pwq = get_work_pwq(work);
	
	if(list_empty(&pwq->pool->worklist))
		pwq->pool->watchdog_ts = jiffies;
	move_linked_works(work, &pwq->pool->worklist, NULL);
	__clear_bit(WORK_STRUCT_DELAYED_BIT, work_data_bits(work));
	pwq->nr_active++;
}

static void pwq_activate_first_delayed(struct pool_workqueue *pwq)
{
	struct work_struct *work = list_
	
	
}