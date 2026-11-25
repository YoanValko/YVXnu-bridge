# AMD64 port status

The upstream project hosts an ARM64 XNU kernel.  Its `fki_mod.c`,
`xnu-ko/world.S`, and XNU patch set directly use ARM64 exception state,
translation tables, timers, TLS registers, and pointer authentication.
Those components cannot execute on an AMD64 Linux kernel.

## What builds on Ubuntu AMD64

`fki-ko/fki_mod_x86.c` is an AMD64 bootstrap implementation of FKI.  It
builds as an out-of-tree module on a stock Ubuntu AMD64 kernel:

```sh
make -C /lib/modules/$(uname -r)/build M="$PWD/fki-ko" modules
```

It supplies the architecture-neutral services needed by the eventual x86 XNU
machine layer: logging, firmware blobs, executable image storage, random data,
clock and wall-clock access, CPU binding, helper threads, and the host-call
device. Blocking I/O, sleep/yield, and packet delivery switch to a per-CPU
Linux-owned alternate stack before re-enabling scheduling. The counter uses
nanoseconds and reports a frequency of 1 GHz; the x86 XNU layer must use that
contract.

## Deliberately unsupported in the bootstrap

The module currently returns `-EOPNOTSUPP` for a foreign page-table window and
foreign fault registration. Registering those interfaces without a complete
x86 exception-entry design would be unsafe, so they remain an explicit stop
point. Those are not optional runtime features:
implementing them requires a matching x86-64 XNU machine layer and careful
CR3/TLB/IDT switching.  Contiguous foreign DRAM allocation is now available,
but it is not sufficient to boot XNU by itself.

The AMD64 build now also produces an `xnu.ko` bring-up loader.  In its default
parse-only mode it validates an x86-64 Mach-O image and does not execute it.
The optional `execute=1` path is only a controlled smoke-test entry point; it
is not a Darwin boot and must not be used with an unadapted macOS kernelcache.
A working native AMD64 system needs all of the following before it can boot a
Darwin userspace:

1. An x86_64 XNU source/configuration and a matching Intel-era macOS userland.
2. A complete x86-64 world switch, beyond the current callee-saved-register
   scaffold in `xnu-ko/world_x86.S` and `world_x86.h`.
3. An x86-64 XNU machine layer replacing the patches under `osfmk/arm64`.
4. FKI implementations for safe CR3 switching, page-table ownership, fault
   interception, interrupt delivery, and contiguous physical memory.

Current ARM64 macOS images cannot become native AMD64 binaries through this
port; they require emulation or virtualization instead.  Intel-era macOS
images still need their x86 XNU machine layer, drivers, kernel collection and
system volume to match this host interface.

## Desktop status

The existing `XnuDisplay` and `XnuInput` kexts are part of the ARM64 hosted
system and depend on its boot arguments, IOKit display ABI and patched XNU
drivers.  The framebuffer converter and input sender are architecture-neutral
host tools, and are now safe to use with a live ARM64 framebuffer dump, but
they do not make the AMD64 parse-only loader a desktop.  An AMD64 desktop
still needs the native x86 XNU machine layer, matching Intel-era drivers and a
matching macOS userland.
