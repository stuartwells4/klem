#
# Module: klem.ko
#
# Description: Wireless 802.11 Kernel Link Emulation
#
# Author: Stuart Wells
#
# (C) Copyright 2013 Stuart Wells
#
#

SRC := ../src/driver/

# Specify adition include paths
EXTRA_CFLAGS=-I$(PWD)/$(SRC) -I$(PWD)
#NOSTDINC_FLAGS := -I$(PWD)

obj-m := klem.o
klem-objs := $(SRC)/klemModule.o $(SRC)/klemProc.o $(SRC)/klemData.o $(SRC)/klemNet.o $(SRC)/klemCtrl.o $(SRC)/klem80211.o
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default: klem

klem:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o .*.cmd *.ko Module.* *.mod.c .tmp_versions modules.order
	rm -rf $(SRC)/*.o
	rm -rf $(SRC)/.*.cmd
