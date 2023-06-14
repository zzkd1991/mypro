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
 * Name: iob_free_queue
 *
 * Description:
 *   Free an entire queue of I/O buffer chains.
 *
 ****************************************************************************/

void iob_free_queue(FAR struct iob_queue_s *qhead,
				enum iob_user_e producerid)
{
	FAR struct iob_qentry_s *iobq;
	FAR struct iob_qentry_s *nextq;
	FAR struct iob_s *iob;

	/* Detach the list from the queue head so first for safety (should be safe
	 * anyway).
	 */

	iobq	= qhead->qh_head;
	qhead->qh_head = NULL;

	/* Remove each I/O buffer chain from the queue */

	while(iobq)
	{
		/* Remove the I/O buffer chain from the head of the queue and
		 * discard the queue container.
		 */

		iob = iobq->qe_head;

		/* Remove the queue container from the list and discard it */

		nextq = iobq->qe_flink;
		iob_free_qentry(iobq);
		iobq = nextq;

		/* Free the I/O chain*/

		iob_free_chain(iob, producerid);
	}
}

#endif

