#include "mm/iob.h"

#include "iob.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_free_chain
 *
 * Description:
 *   Free an entire buffer chain, starting at the beginning of the I/O
 *   buffer chain
 *
 ****************************************************************************/

void iob_free_chain(struct iob_s * iob, enum iob_user_e producerid)
{
	FAR struct iob_s *next;

	/* Free each IOB in the chain -- one at a time to keep the count straight */

	for(; iob; iob = next)
	{
		next = iob_free(iob, producerid);
	}
}

