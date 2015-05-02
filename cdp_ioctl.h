#ifndef CDP_IOCTL_H
#define CDP_IOCTL_H

#define CDP_MODULE_NAME "cdp"
#define CDP_MISC_NAME   "cdp_misc"

#define CDP_NAME_LEN 32

struct cdp_ioctl {
	char name[CDP_NAME_LEN];

	int cdp_host_major_num;
	int cdp_host_minor_num;

	int cdp_repository_major_num;
	int cdp_repository_minor_num;

	int cdp_metadata_major_num;
	int cdp_metadata_minor_num;
};

enum {
	CDP_VERSION_CMD    = 0,
	CDP_DEV_CREATE_CMD = 1
};

#define CDP_IOC_MAGIC 'G'

#define CDP_VERSION    _IOR(CDP_IOC_MAGIC,CDP_VERSION_CMD,struct cdp_ioctl)
#define CDP_DEV_CREATE _IOWR(CDP_IOC_MAGIC,CDP_DEV_CREATE_CMD,struct cdp_ioctl)

#endif
