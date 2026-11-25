# SPDX-License-Identifier: GPL-2.0-only
obj-m += fki.o
fki-y := fki_hostcall.o
fki-$(CONFIG_ARM64) += fki_mod.o
fki-$(CONFIG_X86_64) += fki_mod_x86.o
fki-$(CONFIG_X86_64) += fki_stack_x86.o
