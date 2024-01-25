obj-m += pseudonfs.o
pseudonfs-objs += src/client/client.o src/client/pseudonfs.o
ccflags-y := -std=gnu11 -Wno-declaration-after-statement

pseudonfs-build:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

pseudonfs-clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean


server-build:
	gcc -o server src/server/main.c src/server/fs.c