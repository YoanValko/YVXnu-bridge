#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Install VirGL/Venus development files locally when sudo is unavailable.
# The Ubuntu runtime package may already be system-installed; this keeps the
# headers, pkg-config metadata and linker symlink inside the checkout.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PREFIX=${VIRGL_PREFIX:-$ROOT/.deps/virgl}
CACHE="$PREFIX/cache"
mkdir -p "$CACHE"

if [ ! -f "$PREFIX/usr/include/virgl/virglrenderer.h" ] ||
   [ ! -e "$PREFIX/usr/lib/x86_64-linux-gnu/libvirglrenderer.so" ]; then
  (cd "$CACHE" && apt-get download libvirglrenderer-dev libvirglrenderer1)
  for package in "$CACHE"/*.deb; do
    dpkg-deb -x "$package" "$PREFIX"
  done
fi

test -f "$PREFIX/usr/include/virgl/virglrenderer.h"
test -e "$PREFIX/usr/lib/x86_64-linux-gnu/libvirglrenderer.so"
echo "VirGL installed locally at $PREFIX"
echo "Build with: make -C gpu/xnu-drm"
