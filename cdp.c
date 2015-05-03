/*
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/idr.h>
#include <linux/blkdev.h>

#include "cdp.h"
#include "cdp_ioctl.h"

static int cdp_major;
static atomic_t cdp_misc_ready = ATOMIC_INIT(1);

/*
 * An IDR is used to keep track of allocated minor numbers.
 */
static DEFINE_IDR(_minor_idr);

static DEFINE_SPINLOCK(_minor_lock);


static void free_minor(int minor)
{
	spin_lock(&_minor_lock);
	idr_remove(&_minor_idr, minor);
	spin_unlock(&_minor_lock);
}

static int next_free_minor(int *minor)
{
	int r;

	idr_preload(GFP_KERNEL);
	spin_lock(&_minor_lock);

	r = idr_alloc(&_minor_idr, MINOR_ALLOCED, 0, 1 << MINORBITS, GFP_NOWAIT);

	spin_unlock(&_minor_lock);
	idr_preload_end();
	if (r < 0)
		return r;
	*minor = r;
	return 0;
}

static void cdp_make_request(struct request_queue *q, struct bio *bio)
{
}

static int cdp_blk_open(struct block_device *bdev, fmode_t mode)
{
	return 0;
}

static void cdp_blk_close(struct gendisk *disk, fmode_t mode)
{
}

static int cdp_blk_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct block_device_operations cdp_blk_fops = {
	.open = cdp_blk_open,
	.release = cdp_blk_close,
	.ioctl = cdp_blk_ioctl,
	.owner = THIS_MODULE
};

static int cdp_dev_create(struct cdp_ioctl *param)
{
	int ret;
	int minor;
	struct cdp_device *cd = kzalloc(sizeof(*cd), GFP_KERNEL);

	if (!cd) {
		printk(KERN_ERR "CDP: unable to allocate device, out of memory.\n");
		return -ENOMEM;
	}

	if (!try_module_get(THIS_MODULE))
		goto bad_module_get;

	// get a minor number for the dev
	ret = next_free_minor(&minor);
	if (ret < 0)
		goto bad_minor;

	spin_lock_init(&cd->lock);

	cd->queue = blk_alloc_queue(GFP_KERNEL);
	if (!cd->queue)
		goto bad_queue;

	cd->queue->queuedata = cd;
	blk_queue_make_request(cd->queue, cdp_make_request);

	cd->disk = alloc_disk(1);
	if (!cd->disk)
		goto bad_disk;

	cd->disk->major = cdp_major;
	cd->disk->first_minor = minor;
	cd->disk->fops = &cdp_blk_fops;
	cd->disk->queue = cd->queue;
	cd->disk->private_data = cd;
	sprintf(cd->disk->disk_name, "cdp-%d", minor);
	add_disk(cd->disk);

	return 0;

bad_disk:
	blk_cleanup_queue(cd->queue);
bad_queue:
	free_minor(minor);
bad_minor:
	module_put(THIS_MODULE);
bad_module_get:
	kfree(cd);
	return -ENXIO;
}

static int cdp_validate_params(unsigned int cmd, struct cdp_ioctl *param)
{
	switch (cmd) {
		case CDP_VERSION_CMD:
			return 0;
			break;
		case CDP_DEV_CREATE_CMD:
			if (!param->cdp_host_major_num || !param->cdp_repository_major_num || !param->cdp_metadata_major_num) {
				printk(KERN_ERR "CDP: invalid disk major number.\n");
				return -EINVAL;
			}
			break;
		default:
			break;
	}

	param->name[CDP_NAME_LEN - 1] = '\0';

	return 0;
}

typedef int (*ioctl_fn)(struct cdp_ioctl *param);

/*
 * Dispatch all kinds of ioctl commands
 */
static ioctl_fn ioctl_lookup(unsigned int cmd)
{
	static struct {
		unsigned int cmd;
		ioctl_fn fn;
	}_ioctls[] =
	{
		{CDP_VERSION_CMD, 0},
		{CDP_DEV_CREATE_CMD, cdp_dev_create}
	};

	return (cmd >= ARRAY_SIZE(_ioctls)) ? NULL : _ioctls[cmd].fn;
}

static int cdp_copy_params(struct cdp_ioctl __user *user, struct cdp_ioctl **pparam)
{
	struct cdp_ioctl *cdpi;

	cdpi = kmalloc(sizeof(struct cdp_ioctl), GFP_KERNEL);
	if (!cdpi)
		return -ENOMEM;

	if (copy_from_user(cdpi, user, sizeof(struct cdp_ioctl))) {
		kfree(cdpi);
		return -EFAULT;
	}

	*pparam = cdpi;
	return 0;
}

static void cdp_free_params(struct cdp_ioctl *param)
{
	kfree(param);
}

static int ioctl_main(unsigned int command, struct cdp_ioctl __user *user)
{
	int ret;
	unsigned int cmd;
	struct cdp_ioctl *param = NULL;
	ioctl_fn fn = NULL;

	// only root can play with this
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (_IOC_TYPE(command) != CDP_IOC_MAGIC)
		return -ENOTTY;

	cmd = _IOC_NR(command);

	if(cmd == CDP_VERSION_CMD)
		return 0;

	fn = ioctl_lookup(cmd);
	if (!fn) {
		printk(KERN_ERR "CDP: unknown command 0x%x.\n", cmd);
		return -ENOTTY;
	}

	ret = cdp_copy_params(user, &param);
	if (ret < 0)
		goto out;

	ret = cdp_validate_params(cmd, param);
	if (ret < 0)
		goto out;

	ret = fn(param);

out:
	cdp_free_params(param);
	return ret;
}

static long cdp_misc_ioctl(struct file *file, unsigned int command, unsigned long arg)
{
	return (long)ioctl_main(command, (struct cdp_ioctl __user *)arg);
}

/*
 * Enforce only one user at a time here with the open/release
 */
static int cdp_misc_open(struct inode *inode, struct file *file)
{
	if ( !atomic_dec_and_test(&cdp_misc_ready) ) {
		atomic_inc(&cdp_misc_ready);
		return -EBUSY;
	}

	return 0;
}

static int cdp_misc_release(struct inode *inode, struct file *file)
{
	atomic_inc(&cdp_misc_ready);
	return 0;
}

static const struct file_operations cdp_misc_fops = {
	.unlocked_ioctl = cdp_misc_ioctl,
	.open           = cdp_misc_open,
	.release        = cdp_misc_release,
	.owner          = THIS_MODULE,
};

static struct miscdevice cdp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = CDP_MISC_NAME,
	.fops  = &cdp_misc_fops
};

void cdp_module_unregister(void)
{
	if (cdp_major > 0) 
		unregister_blkdev(cdp_major, CDP_MODULE_NAME);
}

/*
 * Get a major number for the cdp block device
 */
int cdp_module_register(void)
{
	int ret;

	ret = register_blkdev(0, CDP_MODULE_NAME);
	if (ret < 0) {
		printk(KERN_ERR "CDP: cannot register major number.\n");
		return ret;
	}

	cdp_major = ret;

	return 0;
}

static int __init cdp_misc_init(void)
{
	int ret;

	ret = cdp_module_register();
	if (ret)
		return ret;	

	ret = misc_register(&cdp_misc);
	if (ret) {
		printk(KERN_ERR "CDP: cannot register misc device.\n");
		cdp_module_unregister();
		return ret;
	}
	
	return 0;
}

static void __exit cdp_misc_exit(void)
{
	if (misc_deregister(&cdp_misc) < 0)
		printk(KERN_ERR "CDP: cannot deregister misc device.\n");

	cdp_module_unregister();
}

module_init(cdp_misc_init);
module_exit(cdp_misc_exit);

MODULE_LICENSE("Dual BSD/GPL");
