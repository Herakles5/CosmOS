obj-m += cosmos_hv3.o
cosmos_hv3-objs := cosmos_lkm.o build/cosmos_hypervisor_all.o

# The C++ object file is already compiled; we link it here.
# Note: build/cosmos_hypervisor_all.o must have been built beforehand via 
# ./build_hypervisor.sh baremetal, compiled with the kernel memory model!

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
