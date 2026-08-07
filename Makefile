KERNEL_DIR := /lib/modules/$(shell uname -r)/build
BUILD_DIR  := $(CURDIR)/build

.PHONY: all clean

all:
	mkdir -p $(BUILD_DIR)
	cp firstModule.c Kbuild $(BUILD_DIR)
	$(MAKE) -C $(KERNEL_DIR) M=$(BUILD_DIR) modules

clean:
	$(RM) -r -- $(BUILD_DIR)
