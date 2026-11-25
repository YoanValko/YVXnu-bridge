# SPDX-License-Identifier: GPL-2.0-only
# The Linux end (xnudrmd); the macOS ends are kexts/XnuDrm and darwin/ (libxnudrm, built with Mesa).
# With libvirglrenderer installed (pkg-config), xnudrmd also serves a render node that is not virtio-gpu.
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter
# Prefer the project-local package installed by scripts/install-virgl.sh, then
# fall back to a system package if one is already present.
VIRGL_ROOT ?= $(CURDIR)/../../.deps/virgl
VIRGL_LOCAL_PC := $(firstword $(wildcard $(VIRGL_ROOT)/usr/lib/*/pkgconfig))
VIRGL_LOCAL_HEADER := $(wildcard $(VIRGL_ROOT)/usr/include/virgl/virglrenderer.h)
VIRGL_PKG_CONFIG_PATH := $(VIRGL_LOCAL_PC)$(if $(PKG_CONFIG_PATH),:$(PKG_CONFIG_PATH))
VIRGL_SYSROOT := $(if $(VIRGL_LOCAL_HEADER),PKG_CONFIG_SYSROOT_DIR=$(VIRGL_ROOT))
VIRGL_PKG_CONFIG_ENV := PKG_CONFIG_PATH='$(VIRGL_PKG_CONFIG_PATH)' $(VIRGL_SYSROOT)
VIRGL ?= $(shell $(VIRGL_PKG_CONFIG_ENV) pkg-config --exists virglrenderer && echo 1)
SRC = xnudrmd.c
ifeq ($(VIRGL),1)
SRC += xnudrmd_virgl.c
CFLAGS += -DXNUDRM_VIRGL $(shell $(VIRGL_PKG_CONFIG_ENV) pkg-config --cflags virglrenderer)
LDLIBS += $(shell $(VIRGL_PKG_CONFIG_ENV) pkg-config --libs virglrenderer)
endif
xnudrmd: $(SRC) xnudrm_proto.h xnudrmd.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

.PHONY: backend-info test-virgl
backend-info:
	@if [ "$(VIRGL)" = 1 ]; then echo "GPU bridge backend: virtio-gpu + virglrenderer/Venus"; else echo "GPU bridge backend: virtio-gpu only (run scripts/install-virgl.sh for Venus)"; fi
# the backend alone, on the host's Vulkan driver (no fki, no hosted system)
test-virgl: test/virgl_smoke.c xnudrmd_virgl.c xnudrm_proto.h xnudrmd.h
	@if [ "$(VIRGL)" != 1 ]; then echo "VirGL smoke test skipped (VirGL development files unavailable)"; exit 0; fi
	$(CC) $(CFLAGS) -I. -o test/virgl_smoke test/virgl_smoke.c xnudrmd_virgl.c $(LDLIBS)
	test/virgl_smoke
clean:
	rm -f xnudrmd test/virgl_smoke
