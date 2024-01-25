obj-m += pseudonfs.o
pseudonfs-y += src/client/client.o src/client/pseudonfs.o

pseudonfs-build:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

pseudonfs-clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean