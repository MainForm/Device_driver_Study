KERNEL_DIR := /lib/modules/$(shell uname -r)/build
BUILD_DIR  := $(CURDIR)/build

.PHONY: all clean

all:
	echo "Building the character device module..."
	mkdir -p $(BUILD_DIR)
	cp $(CURDIR)/*.[ch] $(BUILD_DIR)
	cp $(CURDIR)/Kbuild $(BUILD_DIR)
	$(MAKE) -C $(KERNEL_DIR) M=$(BUILD_DIR) modules

	echo "Building the character device open test program..."
	gcc -o $(BUILD_DIR)/char_open_test $(BUILD_DIR)/char_open_test.c

	echo "Building the character device read/write test program..."
	gcc -o $(BUILD_DIR)/char_read_write_test $(BUILD_DIR)/char_read_write_test.c

	echo "Building the character device read/write test program..."
	gcc -o $(BUILD_DIR)/char_ioctl_test $(BUILD_DIR)/char_ioctl_test.c
clean:
	$(RM) -r -- $(BUILD_DIR)
