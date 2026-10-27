obj-m += cosmos_hv3.o
cosmos_hv3-objs := cosmos_lkm.o build/cosmos_hypervisor_all.o

# Die C++ Object Datei ist bereits kompiliert, wir linken sie nur mit ein.
# Wichtig: Der Build von build/cosmos_hypervisor_all.o muss VORHER über 
# ./build_hypervisor.sh baremetal passiert sein, aber mit Kernel-Memory-Model!

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
