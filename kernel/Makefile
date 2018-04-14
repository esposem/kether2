obj-m += kecho.o kclient.o

COMMON_FILES:= eth.o stats.o

kecho-y:= \
	$(COMMON_FILES) \
	echo.o

kclient-y:= \
	$(COMMON_FILES) \
	mclient.o

EXTRA_CFLAGS := -Wall -I$(PWD)/include -O2 # $(ADDITIONAL_FLAG)
ccflags-y := -std=gnu99 -Wall -Wno-declaration-after-statement

all:
	-mkdir build
	-touch build/Makefile
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/build src=$(PWD) modules


clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	-rm -rf build
