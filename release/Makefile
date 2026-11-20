#
# Module: klem.ko
#
# Description: Wireless 802.11 Kernel Link Emulation
#
# Author: Stuart Wells
#
# (C) Copyright 2016 - 2026 Stuart Wells
#
#

SRC := ../src/driver/

# Specify adition include paths ($(src) is set by kbuild to this directory)
ccflags-y := -I$(src)/$(SRC) -I$(src)

obj-m := klem.o
klem-y := $(SRC)/klemModule.o $(SRC)/klemProc.o $(SRC)/klemData.o $(SRC)/klemNet.o $(SRC)/klemCtrl.o $(SRC)/klem80211.o
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default: klem

klem:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

.PHONY: default klem install clean

clean:
	rm -rf *.o .*.cmd *.ko *.mod Module.* *.mod.c .tmp_versions modules.order
	rm -rf $(SRC)/*.o
	rm -rf $(SRC)/.*.cmd
