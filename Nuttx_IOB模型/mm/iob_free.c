#include "mm/iob.h"

#include "iob.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

 /****************************************************************************
  * Name: iob_free
  *
  * Description:
  *   Free the I/O buffer at the head of a buffer chain returning it to the
  *   free list.  The link to  the next I/O buffer in the chain is return.
  *
  ****************************************************************************/
FAR struct iob_s *iob_free(FAR struct iob_s *iob,
						enum iob_user_e producterid)
{
	FAR struct iob_s *next = iob->io_flink;
	irqstate_t flags;

	iobinfo("iob=%p io_pktlen=%u io_len=%u next=%p\n", 
		iob, iob->io_pktlen, iob->io_len, next);

	/* Copy the data that only exists in the head of a I/O buffer chain into
	 * the next entry.
	*/

	if(next != NULL)
	{
		/* Copy and decrement the total packet length, being careful to
		 * do nothing too crazy.
		 */

		if(iob->io_pktlen > iob->io_len)
		{
			/* Adjust packet length and move it to the next entry */

			next->io_pktlen = iob->io_pktlen - iob->io_len;
		}
		else
		{
			/* This can only happen if the free entry isn't first entry in the
			 * chain...
			 */

			next->io_pktlen = 0;
		}
		
		iobinfo("next=%p io_pktlen=%u io_len=%u\n",
				next, next->io_pktlen, next->io_len);
	}

	/* Free the I/O buffer by adding it to the head of the free or the
	 * committed list. We don't know what context we are called from so
	 * we use extreme measures to protect the free list: We disable
	 * interrupts very briefly.
	 */

	flags = enter_critical_section();

	/* Which list? If there is a task waiting for an IOB, then put
	 * the IOB on either the free list or on the committed list where
	 * it is reserved for that allocation (and not available to
	 * iob_tryalloc()).
	 */

	if(g_iob_sem.semcount < 0)
	{
		iob->io_flink = g_iob_committed;
	}
	else
	{
		iob->io_flink = g_iob_freelist;
		g_iob_freelist = iob;
	}

	/* Signal that an IOB is available. If there is a thread blocked,
	 * waiting for an IOB, this will wake up exactly one thread. The
	 * semaphore count will correctly indicated that the awakened task
	 * owns an IOB and should find it in the committed list.
	 */

	nxsem_post(&g_iob_sem);

#if CONFIG_IOB_THROTTLE > 0
	nxsem_post(&g_throttle_sem);
#endif

	leave_critcal_section(flags);

	/* And return the I/O buffer after the one that was freed */

	return next;
}


