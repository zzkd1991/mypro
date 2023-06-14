#include "mm/iob.h"

#include "iob.h"

#if CONFIG_IOB_NCHAINS > 0

#ifndef NULL
#  define NULL ((FAR void *)0)
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_remove_queue
 *
 * Description:
 *   Remove and return one I/O buffer chain from the head of a queue.
 *
 * Returned Value:
 *   Returns a reference to the I/O buffer chain at the head of the queue.
 *
 ****************************************************************************/

FAR struct iob_s *iob_remove_queue(struct iob_queue_s * iobq)
{
	FAR struct iob_qentry_s *qentry;
	FAR struct iob_s *iob = NULL;

	/* Remove the I/O buffer chain from the head of the queue */

	irqstate_t flags = enter_critical_section();
	qentry = iobq->qh_head;
	if(qentry)
	{
		iobq->qh_head = qentry->qe_flink;
		if(!iobq->qh_head)
		{
			iobq->qh_tail = NULL;
		}

		/* Extract the I/O buffer chain from the container and free the
		 * container.
		 */

		iob = qentry->qe_head;
		iob_free_qentry(qentry);
	}
	
	leave_critical_section(flags);
	return iob;
}

#endif	/* COFIG_IOB_NCHAINS > 0 */


