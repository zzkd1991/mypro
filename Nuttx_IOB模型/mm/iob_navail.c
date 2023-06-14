#include "mm/iob.h"

#include "iob.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iob_navail
 *
 * Description:
 *   Return the number of available IOBs.
 *
 ****************************************************************************/

int iob_navail(bool throttled)
{
	int navail = 0;
	int ret;

#if CONFIG_IOB_NBUFFERS > 0
	/* Get the value of the IOB counting semaphores */

	ret = nxsem_get_value(&g_iob_sem, &navail);
	if(ret >= 0)
	{
		ret = navail;

#if CONFIG_IOB_THROTTLE > 0
		/* Subtract the throttle value is so requested */

		if(throttled)
		{
			ret -= CONFIG_IOB_THROTTLE;
		}
#endif


		if(ret <0)
		{
			ret = 0;
		}
	}
#else
	ret = navail;
#endif

	return ret;
}

/****************************************************************************
 * Name: iob_qentry_navail
 *
 * Description:
 *   Return the number of available IOB chains.
 *
 ****************************************************************************/
int iob_qentry_navail(void)
{
	int navail = 0;
	int ret;

#if CONFIG_IOB_NCHAINS > 0
	/* Get the value of the IOB chain qentry counting semaphores */

	ret = nxsem_get_value(&g_qentry_sem, &navail);
	if(ret >= 0)
	{
		ret = navail;
		if(ret < 0)
		{
			ret = 0;
		}
	}

#else
	ret = navail;
#endif

	return ret;
}

