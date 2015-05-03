#ifndef CDP_H
#define CDP_H

#define MINOR_ALLOCED ((void *)-1)

struct cdp_device {
	spinlock_t lock;
	struct gendisk *disk;
	struct request_queue *queue;
};

#endif
