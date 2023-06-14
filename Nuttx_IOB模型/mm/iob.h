#ifndef __MM_IOB_IOB_H
#define __MM_IOB_IOB_H

#ifdef CONFIG_MM_IOB

/***********************************************
 * Public Data
 ***********************************************/
 
/* A list of all free, unallocated I/O buffers */
extern struct iob_s *g_iob_freelist;

/* A list of I/O buffers that are committed for allocation */
 extern struct iob_s *g_iob_committed;
 
#if CONFIG_IOB_NCHAINS > 0
/* A list of all free, unallocated I/O buffer queue containers */ 
extern struct iob_qentry_s *g_iob_freeqlist;

extern struct iob_qentry_s *g_iob_qcommitted;
#endif

/* Counting semaphores that tracks the number of free IOBs/qentries */

extern SEM_CB_S g_iob_sem;	/* Counts free I/O buffers */
#if CONFIG_IOB_THROTTLE > 0
extern SEM_CB_S g_throttle_sem; /* Counts available I/O buffers when throttled */
#endif
#if CONFIG_IOB_NCHAINS > 0
extern SEM_CB_S g_qentry_sem; /* Counts free I/O buffer queue containers */


/**********************************************************
 * Public Function Prototypes
 **********************************************************/

/***********************************************************
 * Name: iob_alloc_qentry
 *
 * Description:
 * 	Allocate an I/O buffer chain container by taking the buffer at the head
 *	of the free list. This function is intended only for internal use by
 *	the IOB module.
 ************************************************************/
 
 struct iob_qentry_s *iob_alloc_qentry(void);
 
 
/*************************************************************
 * Name: iob_tryalloc_qentry
 *
 * Description:
 *		Try to allocate an I/O buffer chain container by taking the buffer at
 *		the head of the free list without waiting for the container to become
 *		free. This function is intended only for internal use by the IOB module
 **************************************************************/

struct iob_qentry_s *iob_tryalloc_qentry(void);


/***************************************************************
 * Name: iob_free_qentry
 *
 * Description:
 *		Free the I/O buffer chain container by returning it the free list.
 *		The link to the next I/O buffer in the chain is return
 ***************************************************************/
 
struct iob_qentry_s *iob_free_qentry(struct iob_qentry_s *iobq);


/*****************************************************************
 * Name: iob_notifier_signal
 *
 * Description:
 *		An IOB has become available. Signal all threads waiting for an IOB
 *		that an IOB is available.
 *
 *		When an IOB becomes available, *all* of the waiters in this thread will
 *		be signaled. If there are multiple waiters then only the highest
 *		priority thread will get the IOB. Lower priority threads will need to
 *		call iob_notifier_setup() once again.
 *
 * Input Parameters:
 *	None.
 *
 * Returned Value:
 *	None.
 *
 * Returned Value:
 *	None.
 *
 **************************************************************************************/

#ifdef CONFIG_IOB_NOTIFIER
void iob_notifier_signal(void);
#endif 


#endif /* CONFIG_MM_IOB */
#endif /* __MM_IOB_IOB_H */