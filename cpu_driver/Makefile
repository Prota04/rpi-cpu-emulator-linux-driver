obj-m += cpu_emulator_driver.o

ccflags-y := -I$(src)/..

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean