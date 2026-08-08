KERNEL_DIR := /lib/modules/$(shell uname -r)/build
BUILD_DIR  := $(CURDIR)/build

.PHONY: all clean

all:
	echo "Building the character device module..."
	mkdir -p $(BUILD_DIR)
	cp char_device.c Kbuild $(BUILD_DIR)
	$(MAKE) -C $(KERNEL_DIR) M=$(BUILD_DIR) modules

	echo "Building the test program..."
	gcc -o $(BUILD_DIR)/char_open_test char_open_test.c

clean:
	$(RM) -r -- $(BUILD_DIR)
