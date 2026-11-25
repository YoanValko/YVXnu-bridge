# Contributing to YVXnu-bridge

Thanks for helping improve YVXnu-bridge. The project is experimental kernel and
systems software, so small, reviewable changes are preferred.

## Before you start

1. Read [`docs/amd64-port.md`](docs/amd64-port.md) for the current scope.
2. Do not add macOS system images, Apple-provided binaries, private keys, or
   generated kernel-build output.
3. Keep architecture-specific changes isolated and document host assumptions.

## Local checks

On an Ubuntu x86-64 development host:

```sh
make check
git diff --check
```

The check suite builds the modules and runs host-side tests without loading a
kernel module. Do not use `insmod` on a production machine.

## Pull requests

Include a short summary, the host kernel/toolchain version, and the exact test
commands you ran. If a check cannot run on your machine, explain why. Changes
to the FKI or XNU ABI should include a focused regression test where practical.
