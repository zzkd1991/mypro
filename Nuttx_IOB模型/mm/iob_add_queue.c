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
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_add_queue_internal
 *
 * Description:
 *   Add one I/O buffer chain to the end of a queue.  May fail due to lack
 *   of resources.
 *
 ****************************************************************************/

static int iob_add_queue_internal(FAR struct iob_s *iob,
							FAR struct iob_queue_s *iobq,
							FAR struct iob_qentry_s *qentry)
{
	/*Add the I/O buffer chain to the container */

	qentry->qe_head = iob;

	/*Add the container to the end of the queue*/

	qentry->qe_flink = NULL;

	irqstate_t flags = enter_critical_section();
	if(!iobq->qh_head)
	{
		iobq->qh_head = qentry;
		iobq->qh_tail = qentry;
	}
	else
	{
		iobq->qh_tail->qe_flink = qentry;
		iobq->qh_tail = qentry;
	}

	leave_critical_section(flags);

	return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_add_queue
 *
 * Description:
 *	 Add one I/O buffer chain to the end of a queue.  May fail due to lack
 *	 of resources.
 *
 ****************************************************************************/
int iob_add_queue(FAR struct iob_s *iob, FAR struct iob_queue_s *iobq)
{
	FAR struct iob_qentry_s *qentry;

	/* Allocate a container to hold the I/O buffer chain */

	qentry = iob_alloc_qentry();
	if(!qentry)
	{
		ioberr("ERROR: Failed to allocate a container\n");
		return -ENOMEM;
	}

	return iob_add_queue_internal(iob, iobq, qentry);
}

/****************************************************************************
 * Name: iob_tryadd_queue
 *
 * Description:
 *		Add one I/O buffer chain to the end of a queue without waiting for
 *		resources to become free.
 *
 ******************************************************************************************/

int iob_tryadd_queue(struct iob_s * iob, struct iob_queue_s * iobq)
{
	FAR struct iob_qentry_s *qentry;

	/* Allocate a container to hold the I/O buffer chain */

	qentry = iob_tryalloc_qentry();
	if(!qentry)
	{
		ioberr("ERROR: Failed to allocate a container\n");
		return -ENOMEM;
	}

	return iob_add_queue_internal(iob, iobq, qentry);
}
#endif /* CONFIG_IOB_NCHAINS >0*/

