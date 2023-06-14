#include "mm/iob.h"

#include "iob.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_alloc_committed
 *
 * Description:
 *   Allocate an I/O buffer by taking the buffer at the head of the committed
 *   list.
 *
 ****************************************************************************/

static FAR struct iob_s *iob_alloc_committed(enum iob_user_e consumerid)
{
	FAR struct iob_s *iob = NULL;
	irqstate_t flags;

	/* We don't know what context we are called from so we use extreme measures
	 * to protect the committed list: We disable interrupts very briefly.
	 */

	flags = enter_critical_section();

	/* Take the I/O buffer from the head of the committed list */

	iob = g_iob_committed;
	if(iob != NULL)
	{
		/* Remove the I/O buffer from the committed list */

		g_iob_committed = iob->io_flink;

		/* Put the I/O buffer in a known state */

		iob->io_flink = NULL;		/* Not in a chain */
		iob->io_len = 0;			/* Length of the data in the entry */
		iob->io_offset = 0;			/* Offset to the beginning of data */
		iob->io_pktlen = 0;			/* Total length of the packet */
	}

	leave_critical_section(flags);
	return iob;
}

/****************************************************************************
 * Name: iob_allocwait
 *
 * Description:
 *   Allocate an I/O buffer, waiting if necessary.  This function cannot be
 *   called from any interrupt level logic.
 *
 ****************************************************************************/

static FAR struct iob_s *iob_allocwait(bool throttled, unsigned int timeout,
									enum iob_user_e consumerid)
{
	FAR struct iob_s *iob;
	irqstate_t flags;
	FAR sem_t *sem;
	int ret = OK;

#if CONFIG_IOB_THROTTLE > 0
	/* Select the semaphore count to check. */

	sem = (throttled ? &g_throttle_sem : &g_iob_sem);
#else
	sem = &g_iob_sem;
#endif

  /* The following must be atomic; interrupt must be disabled so that there
   * is no conflict with interrupt level I/O buffer allocations.  This is
   * not as bad as it sounds because interrupts will be re-enabled while
   * we are waiting for I/O buffers to become free.
   */

	flags = enter_critical_section();

	/* Try to get an I/O buffer. If successful, the semaphore count will be
	 * decremented atomically.
	*/

	iob = iob_tryalloc(throttled, consumerid);
	while(ret == OK && iob == NULL)
	{
		/* If not successful, then the semaphore count was less that or equal
		 * to zero (meaning that there are no free buffers). We need to wait
		 * for an I/O buffer to be released and placed in the committed
		 * list.
		*/

		if(timeout == UINT_MAX)
		{
			ret = nxsem_wait_uninterruptible(sem);
		}
		else
		{
			ret = nxsem_tickwait_uninterruptible(sem, MSEC2TICK(timeout));
		}

		if(ret >= 0)
		{
			/* When we wake up from wait successfully, an I/O buffer was
			 * freed and we hold a count for one IOB
			 */

			iob = iob_alloc_committed(consumerid);
			if(iob == NULL)
			{
				/* We need release our count so that it is available to
				 * iob_tryalloc(), perhaps allowing another thread to take our
				 * count. In that event, iob_tryalloc() will fail above and
				 * we will have to wait again.
				*/
				
				nxsem_post(sem);
				iob = iob_tryalloc(throttled, consumerid);
			}

			/* REVISIT: I think this logic should be moved inside of
			 * iob_alloc_committed, so that it can exist inside of the critical
			 * section along with all other sem count changes.
			*/

#if CONFIG_IOB_THROTTLE > 0
			else
			{
				if(throttled)
				{
					g_iob_sem.semcount--;
				}
				else
				{
					g_throttle_sem.semcount--;
				}
			}
#endif
		}
	}

	leave_cirtical_section(flags);
	return iob;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_timedalloc
 *
 * Description:
 *	Allocate an I/O buffer by taking the buffer at the head of the free list.
 *	This wait will be terminated when the specified timeout expires.
 *
 * Input Parameters:
 *	 throttled	- An indication of the IOB allocation is "throttled"
 *	 timeout	- Timeout value in milliseconds.
 *	 consumerid - id representing who is consuming the IOB
 *
 ****************************************************************************/

FAR struct iob_s *iob_timedalloc(bool throttled, unsigned int timeout,
								enum iob_user_e consumerid)
{
	/*Were we called from the interrupt level? */
	if(up_interrupt_context() || timeout == 0)
	{
		/* Yes, then try to allocate an I/O buffer without waiting */
		return iob_tryalloc(throttled, consumerid);
	}
	else
	{
		/* Then allocate an I/O buffer, waiting as necessary */
		return iob_allocwait(throttled, timeout, consumerid);
	}

}

								

