KERNEL_DIR := /lib/modules/$(shell uname -r)/build
BUILD_DIR  := $(CURDIR)/build

.PHONY: all clean

all:
	echo "Building the i2c device module..."
	mkdir -p $(BUILD_DIR)
	cp $(CURDIR)/*.[ch] $(BUILD_DIR)
	cp $(CURDIR)/Kbuild $(BUILD_DIR)
	$(MAKE) -C $(KERNEL_DIR) M=$(BUILD_DIR) modules

clean:
	$(RM) -r -- $(BUILD_DIR)
