#include <stdint.h>
#include <string.h>

#include "mm/iob.h"

#include "iob.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef MIN
#  define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_copyout
 *
 * Description:
 *  Copy data 'len' bytes of data into the user buffer starting at 'offset'
 *  in the I/O buffer, returning that actual number of bytes copied out.
 *
 ****************************************************************************/

int iob_copyout(FAR uint8_t *dest, FAR const struct iob_s *iob,
			unsigned int len, unsigned int offset)
{
	FAR const uint8_t *src;
	unsigned int ncopy;
	unsigned int avail;
	unsigned int remaining;

	/*Skip to the I/O buffer containing the offset */

	while(offset >= iob->io_len)
	{
		offset -= iob->io_len;
		iob		= iob->io_flink;
		if(iob == NULL)
		{
			/*We have no requested data in iob chain */

			return 0;
		}
	}

	/* Then loop until all of the I/O data is copied to the user buffer */

	remaining = len;
	while(iob && remaining > 0)
	{
		/* Get the source I/O buffer offset address and the amount of data
		 * available from that address
		 */

		src = &iob->io_data[iob->io_offset + offset];
		avail = iob->io_len - offset;

		/* Copy the from the I/O buffer in to the user buffer */

		ncopy = MIN(avail, remaining);
		memcpy(dest, src, ncopy);

		/*Adjust the total length of the copy and the destination address in
		 * the user buffer.
		 */

		remaining -= ncopy;
		dest += ncopy;

		/* Skip to the next I/O buffer in the chain*/

		iob = iob->io_flink;
		offset = 0;
	}


	return len - remaining;
}


