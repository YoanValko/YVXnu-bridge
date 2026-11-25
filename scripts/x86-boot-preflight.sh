#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Safe AMD64 bring-up preflight. This never inserts a module or executes XNU.
set -euo pipefail
cd "$(dirname "$0")/.."

if [ "$(uname -m)" != x86_64 ]; then
  echo "This preflight requires an x86-64 Linux guest (found $(uname -m))." >&2
  exit 2
fi

if [ -d /sys/module/fki ] || [ -d /sys/module/xnu ]; then
  echo "Refusing to run while fki or xnu is loaded; stop the experiment first." >&2
  exit 1
fi

make check VIRGL="${VIRGL:-0}"

if [ -d /sys/module/fki ] || [ -d /sys/module/xnu ]; then
  echo "Unexpected module load detected." >&2
  exit 1
fi

echo "AMD64 boot preflight: PASS"
echo "No kernel module was loaded and no XNU image was executed."
echo "A native x86-64 XNU machine layer and purpose-built test image are still required for boot."
