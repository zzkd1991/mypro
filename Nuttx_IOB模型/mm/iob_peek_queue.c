#include "mm/iob.h"

#include "iob.h"

#if CONFIG_IOB_NCHAINS > 0

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef NULL
#  define NULL ((FAR void *)0)
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_peek_queue
 *
 * Description:
 *   Return a reference to the I/O buffer chain at the head of a queue. This
 *   is similar to iob_remove_queue except that the I/O buffer chain is in
 *   place at the head of the queue.  The I/O buffer chain may safely be
 *   modified by the caller but must be removed from the queue before it can
 *   be freed.
 *
 * Returned Value:
 *   Returns a reference to the I/O buffer chain at the head of the queue.
 *
 ****************************************************************************/

FAR struct iob_s *iob_peek_queue(FAR struct iob_queue_s *iobq)
{
	FAR struct iob_qentry_s *qentry;
	FAR struct iob_s *iob = NULL;

	/* Peek at the I/O buffer chain container at the head of the queue */

	qentry = iobq->qh_head;

	if(qentry)
	{
		/* Return the I/O buffer chain from the container */
		
		iob = qentry->qe_head;
	}

	return iob;
}

#endif


