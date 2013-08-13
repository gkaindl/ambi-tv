obj-m += usbtv.o

KVERSION := $(shell uname -r)

all:
	make -C /lib/modules/$(KVERSION)/build $(INCLUDE) M=$(PWD) modules
install:
	make -C /lib/modules/$(KVERSION)/build $(INCLUDE) M=$(PWD) modules_install
clean:
	make -C /lib/modules/$(KVERSION)/build $(INCLUDE) M=$(PWD) clean

