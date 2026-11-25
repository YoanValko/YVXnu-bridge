# SPDX-License-Identifier: GPL-2.0-only
# The native ARM64 build has additional external toolchain and XNU inputs.
# On AMD64, make builds the host-side FKI module and the conservative x86
# Mach-O bring-up loader documented in docs/amd64-port.md.

KDIR ?= /lib/modules/$(shell uname -r)/build
M      := $(CURDIR)/fki-ko

.XNU_M := $(CURDIR)/xnu-ko

.PHONY: all check amd64-fki amd64-xnu gpu-check hosted-desktop-check x86-world-check x86-macho-check x86-world-test x86-stack-test x86-abi-check clean

all: amd64-fki

# Complete non-loading validation for an Ubuntu AMD64 checkout.  Nothing in
# this target calls insmod/rmmod or opens the host GPU device.
check: amd64-fki gpu-check hosted-desktop-check x86-macho-check x86-world-test x86-stack-test x86-abi-check x86-world-check
	@bash -n scripts/xnulinux-boot.sh scripts/xnu-console.sh
	@if [ -d /sys/module/fki ] || [ -d /sys/module/xnu ]; then \
		echo "Warning: fki/xnu is already loaded; tests did not load it"; \
	else \
		echo "Kernel modules: not loaded"; \
	fi
	@echo "All non-loading checks: PASS"

amd64-fki:
	@if [ "$(shell uname -m)" != "x86_64" ]; then \
		echo "This target is for x86_64 Ubuntu hosts (detected $(shell uname -m))."; \
		exit 2; \
	fi
	$(MAKE) -C $(KDIR) M=$(M) modules
	@echo "Built $(M)/fki.ko (AMD64 FKI host backend)."
	$(MAKE) -C $(KDIR) M=$(.XNU_M) KBUILD_EXTRA_SYMBOLS=$(M)/Module.symvers modules
	@echo "Built $(.XNU_M)/xnu.ko (AMD64 parse-only Mach-O bring-up loader)."

amd64-xnu: amd64-fki

gpu-check:
	$(MAKE) -C gpu/xnu-drm
	$(MAKE) -C gpu/xnu-drm backend-info
	$(MAKE) -C gpu/xnu-drm test-virgl
	@echo "GPU bridge daemon: PASS"

hosted-desktop-check:
	python3 -m py_compile tools/hosted/fb2png.py tools/hosted/xnu-input.py
	@echo "Hosted desktop tools: PASS"

x86-world-check:
	$(CC) -c -ffreestanding -fno-stack-protector -o /tmp/xnulinux-world_x86.o xnu-ko/world_x86.S
	@echo "Assembled xnu-ko/world_x86.S"

x86-macho-check:
	$(CC) -std=c11 -Wall -Wextra -I fki-ko -I xnu-ko tests/macho_x86_test.c xnu-ko/macho.c -o /tmp/xnulinux-macho_x86_test
	/tmp/xnulinux-macho_x86_test

x86-world-test:
	$(CC) -DUSER_TEST -fno-pie -no-pie -fomit-frame-pointer -I fki-ko -I xnu-ko tests/world_x86_test.c xnu-ko/world_x86.S tests/world_x86_entry.S -o /tmp/xnulinux-world_x86_test
	/tmp/xnulinux-world_x86_test

x86-stack-test:
	$(CC) -DUSER_TEST -fno-pie -no-pie -fomit-frame-pointer tests/stack_x86_test.c fki-ko/fki_stack_x86.S -o /tmp/xnulinux-stack_x86_test
	/tmp/xnulinux-stack_x86_test

x86-abi-check:
	$(CC) -std=c11 -Wall -Wextra -fsyntax-only -I xnu-patches/include tests/linux64_x86_test.c
	@echo "linux64 x86-64 ABI syntax check: PASS"

clean:
	$(MAKE) -C $(KDIR) M=$(M) clean
	$(MAKE) -C $(KDIR) M=$(.XNU_M) clean
