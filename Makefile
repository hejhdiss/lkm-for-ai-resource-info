# NeuroShell Kernel Module Makefile
# Build both original and enhanced versions

obj-m += neuroshell.o
obj-m += neuroshell_enhanced.o
ccflags-y := -mavx512f

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: all clean install uninstall test help

all:
	@echo "Building NeuroShell kernel modules..."
	make -C $(KDIR) M=$(PWD) modules

clean:
	@echo "Cleaning build artifacts..."
	make -C $(KDIR) M=$(PWD) clean
	rm -f Module.symvers modules.order

install: all
	@echo "Installing NeuroShell module..."
	sudo insmod neuroshell_enhanced.ko
	@echo "Module loaded. Check /sys/kernel/neuroshell/"

uninstall:
	@echo "Removing NeuroShell module..."
	-sudo rmmod neuroshell_enhanced 2>/dev/null || true
	-sudo rmmod neuroshell 2>/dev/null || true
	@echo "Module unloaded."

test: install
	@echo ""
	@echo "=== NeuroShell System Information ==="
	@echo ""
	@cat /sys/kernel/neuroshell/system_summary
	@echo ""
	@echo "Available attributes in /sys/kernel/neuroshell/:"
	@ls -1 /sys/kernel/neuroshell/

reload: uninstall install

info:
	@echo "Checking module info..."
	@modinfo neuroshell_enhanced.ko 2>/dev/null || echo "Module not built yet. Run 'make' first."

help:
	@echo "NeuroShell Kernel Module - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build the kernel module (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Load the module into the kernel"
	@echo "  uninstall  - Unload the module from the kernel"
	@echo "  test       - Install and display system info"
	@echo "  reload     - Unload and reload the module"
	@echo "  info       - Display module information"
	@echo "  help       - Display this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make              # Build the module"
	@echo "  make install      # Load into kernel"
	@echo "  make test         # Test the module"
	@echo "  make uninstall    # Remove from kernel"
