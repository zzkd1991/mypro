#include "mm/iob.h"

#include "iob.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_tailroom
 *
 * Description:
 *  Return the number of bytes at the tail of the I/O buffer chain which
 *  can be used to append data without additional allocations.
 *
 ****************************************************************************/

unsigned int iob_tailroom(FAR struct iob_s * iob)
{
	while(iob->io_flink != NULL)
	{
		iob = iob->io_flink;
	}

	return CONFIG_IOB_BUFSIZE - (iob->io_offset + iob->io_len);
}


