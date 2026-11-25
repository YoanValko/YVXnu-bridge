# SPDX-License-Identifier: APSL-2.0
# xnu.ko: the APSL side. Out-of-tree, non-GPL module that only uses fki.h.
obj-m += xnu.o
ifeq ($(CONFIG_X86_64),y)
xnu-y := xnu_main_x86.o macho.o adt.o world_x86.o world_x86_data.o
else
xnu-y := xnu_main.o macho.o adt.o world.o
endif
ccflags-y += -Wno-declaration-after-statement
