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
 * Name: iob_free_queue_qentry
 *
 * Description:
 *   Free an iob entire queue of I/O buffer chains.
 *
 ****************************************************************************/

void iob_free_queue_qentry(FAR struct iob_s * iob, FAR struct iob_queue_s * iobq, enum iob_user_e producerid)
{
	FAR struct iob_qentry_s *prev = NULL;
	FAR struct iob_qentry_s *qentry;

	irqstate_s flags = enter_critical_section();

	for(qentry = iobq->qh_head; qentry != NULL;
		prev = qentry, qentry = qentry->qe_flink)
	{
		/*Find head of the I/O buffer chain */

		if(qentry->qe_head == iob)
		{
			if(prev == NULL)
			{
				iobq->qh_head = qentry->qe_flink;
			}
			else
			{
				prev->qe_flink = qentry->qe_flink;
			}

			if(iobq->qh_tail == qentry)
			{
				iobq->qh_tail = prev;
			}

			/*Remove the queue container */

			iob_free_qentry(qentry);

			/*Free the I/O chain*/

			iob_free_chain(iob, producerid);

			break;
		}
	}

	leave_critical_section(flags);	
}

#endif

