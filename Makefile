TARGET = cdp
OBJS = cdp.o

CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)

obj-m      := $(TARGET).o

default:
	make -C $(KDIR) SUBDIRS=$(PWD) modules

$(TARGET).o: $(OBJS)
	$(LD) $(LD_RFLAG) -r -o $@ $(OBJS)

clean:
	make -C $(KDIR) SUBDIRS=$(PWD) clean
	rm -rf Module.markers modules.order Module.symvers

-include $(KDIR)/Rules.make
