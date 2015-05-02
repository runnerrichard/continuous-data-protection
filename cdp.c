/*
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

#include "cdp_ioctl.h"

static int cdp_major;
static atomic_t cdp_misc_ready = ATOMIC_INIT(1);

static long cdp_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
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
