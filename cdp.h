#ifndef CDP_H
#define CDP_H

/*
 * Bits for the cd->flags field.
 */
#define CDF_DELETING 4

struct cdp_device {
	spinlock_t lock;
	atomic_t holders;
	atomic_t open_count;

	unsigned long flags;

	struct gendisk *disk;
	struct request_queue *queue;
};

#endif
