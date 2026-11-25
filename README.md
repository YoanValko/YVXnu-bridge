<div align="center">
  <h1>YVXnu-bridge</h1>
  <p><strong>Experimental XNU-on-Linux hosting and GPU integration</strong></p>
  <p>
    <a href="https://github.com/YoanValko/YVXnu-bridge/stargazers"><img src="https://img.shields.io/github/stars/YoanValko/YVXnu-bridge?style=flat-square&label=stars" alt="GitHub stars"></a>
    <a href="https://github.com/YoanValko/YVXnu-bridge/issues"><img src="https://img.shields.io/github/issues/YoanValko/YVXnu-bridge?style=flat-square&label=issues" alt="GitHub issues"></a>
    <a href="https://github.com/YoanValko/YVXnu-bridge/actions/workflows/check.yml"><img src="https://github.com/YoanValko/YVXnu-bridge/actions/workflows/check.yml/badge.svg?branch=main" alt="Checks"></a>
    <img src="https://img.shields.io/badge/platform-x86__64%20%7C%20ARM64-2f6f9f?style=flat-square" alt="Platforms: x86-64 and ARM64">
    <img src="https://img.shields.io/badge/status-experimental-d97706?style=flat-square" alt="Status: experimental">
  </p>
  <p>
    <a href="#quick-start-ubuntu-x86-64">Quick start</a> ·
    <a href="docs/amd64-port.md">AMD64 port</a> ·
    <a href="docs/hosted-service.md">Hosted service</a> ·
    <a href="docs/production.md">Known limitations</a>
  </p>
</div>

YVXnu-bridge is an experimental systems-research project exploring how Apple's
open-source XNU kernel can be hosted alongside a Linux kernel through a small
foreign-kernel interface (FKI). It also contains Linux-side display, input,
networking, storage, and GPU-bridge pieces for the ARM64 hosted-system path.

> **Project status:** experimental research software. The AMD64 port currently
> provides a safe, parse-only XNU bring-up loader and non-loading validation
> tests. It does **not** boot macOS on a generic x86-64 Linux PC.

## Start safely

Build and test the current AMD64 port without loading a kernel module:

```sh
git clone https://github.com/YoanValko/YVXnu-bridge.git && \
  cd YVXnu-bridge && \
  make check
```

`make check` is the supported first test. It builds the modules, runs the host
tests, and leaves your running Linux kernel unchanged.

## What works today

| Area | Status |
| --- | --- |
| Ubuntu/Linux x86-64 build | Builds `fki.ko`, the AMD64 FKI backend, and `xnu.ko` |
| AMD64 XNU loader | Mach-O validation and ABI bring-up; parse-only by design |
| AMD64 validation | Mach-O, stack-switch, world-switch, ABI, and assembly checks |
| Linux GPU bridge | Virtio-GPU + VirGL/Venus userspace bridge with smoke tests |
| Hosted display tools | Framebuffer conversion and validated keyboard/pointer input |
| ARM64 hosted path | Existing experimental path; see [`docs/`](docs/) for platform-specific requirements |

YVXnu-bridge does not distribute macOS, Apple system disks, Apple kexts, or other
Apple-only binaries. Those components remain the responsibility of the user,
subject to Apple's licences and applicable law.

## Architecture

```text
                    hosted XNU / Darwin code
                              |
                         xnu.ko (APSL)
                              |
                  foreign kernel interface (FKI)
                              |
                         fki.ko (GPL)
                              |
                       Linux kernel / drivers
                              |
            display · input · storage · network · GPU bridge
```

The AMD64 path intentionally stops before machine-layer execution. This keeps
the current bring-up work useful for validating image formats and calling
conventions without asking a normal Linux installation to execute an
unsupported kernel image.

## Quick start: Ubuntu x86-64

Install the build dependencies:

```sh
sudo apt update
sudo apt install -y \
  git build-essential pkg-config python3 \
  linux-headers-$(uname -r) \
  libdrm-dev libvirglrenderer-dev libvirglrenderer1
```

Clone the project and run the complete non-loading check suite:

```sh
git clone https://github.com/YoanValko/YVXnu-bridge.git
cd YVXnu-bridge
make check
```

`make check` builds the modules, compiles the GPU bridge, checks the hosted
desktop tools, and runs the AMD64 tests. It does **not** call `insmod`,
`rmmod`, or touch the host GPU. The generated modules are:

```text
fki-ko/fki.ko
xnu-ko/xnu.ko
```

If VirGL development files are unavailable and you cannot install system
packages, use the project-local helper instead:

```sh
./scripts/install-virgl.sh
make check
```

## Safety

The AMD64 loader is parse-only. Do not load it into the kernel on a main or
production machine:

```sh
# Not supported on a normal host:
# sudo insmod fki-ko/fki.ko
# sudo insmod xnu-ko/xnu.ko
```

Kernel-module experiments belong in a disposable VM or a machine with a tested
recovery path. A successful build is not evidence that an XNU image can boot
on the host.

## Useful targets

```sh
make                 # build the kernel modules and GPU daemon
make check           # complete non-loading validation suite
make gpu-check       # build and smoke-test the Linux GPU bridge
make hosted-desktop-check
make x86-macho-check x86-world-test x86-stack-test x86-abi-check x86-world-check
```

For implementation details, see:

- [`docs/amd64-port.md`](docs/amd64-port.md) — AMD64 scope and limitations
- [`docs/hosted-service.md`](docs/hosted-service.md) — ARM64 hosted-service workflow
- [`docs/production.md`](docs/production.md) — known stopgaps and production gaps
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — development and pull-request workflow
- [`SECURITY.md`](SECURITY.md) — safe testing and vulnerability reporting

## Repository layout

| Directory | Purpose |
| --- | --- |
| `fki-ko/` | Linux-side foreign-kernel interface module |
| `xnu-ko/` | XNU loader and architecture-specific bring-up code |
| `xnu-patches/` | Shared Linux↔XNU ABI and machine-layer patches |
| `kexts/` | Hosted display, input, storage, networking, and DRM components |
| `gpu/xnu-drm/` | Linux-side DRM/Venus bridge daemon |
| `scripts/` | Build, boot, and local dependency helpers |
| `tools/hosted/` | Framebuffer and input utilities |
| `tests/` | Host-side ABI and architecture validation tests |

## Contributing

Please keep changes small and architecture-specific where possible. Before
opening a pull request:

```sh
make check
git diff --check
```

When changing a kernel module, include the host kernel version and the exact
test target you ran. Never include macOS system images, private keys, or other
Apple-provided binaries in a pull request.

## Licensing

This repository contains components under multiple licences. See
[`LICENSES/`](LICENSES/) and the SPDX headers in each source file before
redistributing or combining components. YVXnu-bridge is not affiliated with or
endorsed by Apple; macOS, XNU, and Metal are Apple trademarks.

## Acknowledgements

The project builds on ideas and open-source work from
[macOS-DroidVM](https://github.com/Droid-VM/macOS-DroidVM),
[UniXNU](https://github.com/UniXNU),
[Mesa](https://mesa3d.org),
[virglrenderer](https://gitlab.freedesktop.org/virgl/virglrenderer), and the
Apple XNU open-source releases.
