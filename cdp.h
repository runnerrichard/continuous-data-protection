#ifndef CDP_H
#define CDP_H

#define MINOR_ALLOCED ((void *)-1)

struct cdp_device {
	spinlock_t lock;
	atomic_t holders;
	atomic_t open_count;

	struct gendisk *disk;
	struct request_queue *queue;
};

#endif
