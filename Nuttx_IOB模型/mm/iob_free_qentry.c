#include "mm/iob.h"

#include "iob.h"

#if CONFIG_IOB_NCHAINS > 0

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_free_qentry
 *
 * Description:
 *   Free the I/O buffer chain container by returning it to the free list.
 *   The link to  the next I/O buffer in the chain is return.
 *
 ****************************************************************************/

FAR struct iob_qentry_s *iob_free_qentry(FAR struct iob_qentry_s *iobq)
{
	FAR struct iob_qentry_s *nextq = iobq->qe_flink;
	irqstate_t flags;

	/* Free the I/O buffer chain container by adding it to the head of the
	 * free or the committed list. We don't know what context we are called
	 * from so we use extreme measures to protect the free list: We disable
	 * interrupts very briefly.
	 */

	flags = enter_critical_section();

	/* Which list? If there is a task waiting for an IOB chain, then put
	 * the IOB chain on either the free list or on the committed list where
	 * it is reserved for that allocation (and not available to
	 * iob_tryalloc_qentry())
	 */

	if(g_qentry_sem.semcount < 0)
	{
		iobq->qe_flink = g_iob_qcommitted;
		g_iob_qcommitted = iobq;
	}
	else
	{
		iobq->qe_flink = g_iob_freelist;
		g_iob_freeqlist = iobq;
	}

	/* Signal that an I/O buffer chain container is available. If there
	 * is a thread waiting for an I/O buffer chain container, this will
	 * wake up exactly one thread. The semaphore count will correctly
	 * indicated that the awakened task owns an I/O buffer chain container
	 * and should find it in the committed list.
	 */

	nxsem_post(&g_qentry_sem);
	leave_critical_section(flags);


	/* And return the I/O buffer chain container after the one that was freed */

	return nextq;
}

#endif /* CONFIG_IOB_NCHAINS > 0 */


