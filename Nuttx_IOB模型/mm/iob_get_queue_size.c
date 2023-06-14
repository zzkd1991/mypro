#include "mm/iob.h"

#include "iob.h"

#if CONFIG_IOB_NCHAINS > 0


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_get_queue_size
 *
 * Description:
 *   Queue helper for get the iob queue buffer size.
 *
 ****************************************************************************/
unsigned int iob_get_queue_size(FAR struct iob_queue_s * queue)
{
	FAR struct iob_qentry_s *iobq;
	unsigned int total = 0;
	FAR struct iob_s *iob;

	for(iobq = queue->qh_head; iobq != NULL; iobq = iobq->qe_flink)
	{
		iob = iobq->qe_head;
		total += iob->io_pktlen;
	}

	return total;
}

