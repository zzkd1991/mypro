#include "mm/iob.h"

#include "iob.h"

#if CONFIG_IOB_NCHAINS > 0

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_alloc_qcommitted
 *
 * Description:
 *   Allocate an I/O buffer by taking the buffer at the head of the committed
 *   list.
 *
 ****************************************************************************/
static FAR struct iob_qentry_s *iob_alloc_qcommitted(void)
{
	FAR struct iob_qentry_s *iobq = NULL;
	irqstate_t flags;

	/* We don't know what context we are called from so we use extreme measures
	 * to protect the committed list: We disable interrupts very briefly.
	*/

	flags = enter_critical_section();

	/* Take the I/O buffer from the head of the committed list */

	iobq = g_iob_qcommitted;
	if(iobq != NULL)
	{
		/*Remove the I/O buffer from the committed list */

		g_iob_qcommitted = iobq->qe_flink;

		/* Put the I/O buffer in a known state */

		iobq->qe_head = NULL; /* Nothing is contained */
	}

	leave_critical_section(flags);
	return iobq;
}

/****************************************************************************
 * Name: iob_allocwait_qentry
 *
 * Description:
 *   Allocate an I/O buffer chain container by taking the buffer at the head
 *   of the free list.  This function is intended only for internal use by
 *   the IOB module.
 *
 ****************************************************************************/

static FAR struct iob_qentry_s *iob_allocwait_qentry(void)
{
	FAR struct iob_qentry_s *qentry;
	irqstate_t flags;
	int ret = OK;

  /* The following must be atomic; interrupt must be disabled so that there
   * is no conflict with interrupt level I/O buffer chain container
   * allocations.  This is not as bad as it sounds because interrupts will be
   * re-enabled while we are waiting for I/O buffers to become free.
   */

  	flags = enter_critical_section();

	/* Try to get an I/O buffer chain container. If successful, the semaphore
	 * count will bedecremented atomically.
	 */

	qentry = iob_tryalloc_qentry();
	while(ret == OK && qentry == NULL)
	{
		/* If not successful, then the semaphore count was less than or equal
		 * to zero (meaning that there are no free buffers). We need to wait
		 * for an I/O buffer chain container to be released when the
		 * semaphore count will be incremented.
		 */

		ret = nxsem_wait_uninterruptible(&g_qentry_sem);
		if(ret >= 0)
		{
			/* When we wake up from wait successfully, an I/O buffer chain
			 * container was freed and we hold a count for one IOB. Unless
			 * something failed, we should have an IOB waiting for us in the 
			 * committed list.
			 */
			qentry = iob_alloc_qcommitted();

			if(qentry == NULL)
			{
				/* This should not fail, but we allow for that possibility to
				 * handle any potential, non-obvious race condition. Perhaps
				 * the free IOB ended up in the g_iob_free list?
				 *
				 * We need release our count so that it is available to
				 * iob_tryalloc(), perhaps allowing another thread to take our
				 * count. In that event, iob_tryalloc() will fail above and
				 * we will have to wait again.
				*/

				nxsem_post(&g_qentry_sem);
				qentry = iob_tryalloc_qentry();
			}
		}
	}

	leave_critical_section(flags);
	return qentry;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_alloc_qentry
 *
 * Description:
 *   Allocate an I/O buffer chain container by taking the buffer at the head
 *   of the free list. This function is intended only for internal use by
 *   the IOB module.
 *
 ****************************************************************************/
FAR struct iob_qentry_s *iob_alloc_qentry(void)
{
	/* Were we called from the interrupt level? */

	if(up_interrupt_context())
	{
		/* Yes, then try to allocate an I/O buffer without waiting */

		return iob_tryalloc_qentry();
	}
	else
	{
		/* Then allocate an I/O buffer, waiting as necessary */

		return iob_allocwait_qentry();
	}
}

/****************************************************************************
 * Name: iob_tryalloc_qentry
 *
 * Description:
 *   Try to allocate an I/O buffer chain container by taking the buffer at
 *   the head of the free list without waiting for the container to become
 *   free. This function is intended only for internal use by the IOB module.
 *
 ****************************************************************************/

FAR struct iob_qentry_s *iob_tryalloc_qentry(void)
{
	FAR struct iob_qentry_s *iobq;
	irqstate_t flags;

	/* We don't know what context we are called from so we use extreme measures
	 * to protect the free list: We disable interrupts very briefly.
	*/

	flags = enter_critical_section();
	iobq = g_iob_freelist;
	if(iobq)
	{
		/* Remove the I/O buffer chain container from the free list and
		 * decrement the counting semaphore that traces the number of free
		 * containers.
		 */

		g_iob_freelist = iobq->qe_flink;

		/* Take a semaphore count. Note that we cannot do this in
		 * in the orthodox way be calling nxsem_wait() or nxsem_trywait()
		 * because this function may be called from an interrupt
		 * handler. Fortunately we know at at least one free buffer
		 * so a simple decrement is all that is needed.
		 */

		g_qentry_sem.semcount--;

		/* Put the I/O buffer in a know state */

		iobq->qe_head = NULL; /* Nothing is contained */
	}

	leave_critical_section(flags);
	return iobq;
}


#endif /* CONFIG_IOB_NCHAINS > 0 */

