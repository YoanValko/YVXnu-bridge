// SPDX-License-Identifier: APSL-2.0
/*
 * AMD64 bring-up loader.
 *
 * This is intentionally a small, conservative first machine layer.  It can
 * parse and stage an x86-64 Mach-O image from Linux firmware without entering
 * it.  `execute=1` is an explicit developer-only smoke test: it transfers
 * control to the image through the x86 world switch, so it must only be used
 * with a purpose-built test image and a VM snapshot.
 */
#include <linux/module.h>
#include "xrt.h"
#include "macho.h"
#include "world_x86.h"

static char *image = "xnu/kernel.macho";
module_param(image, charp, 0444);
static int execute;
module_param(execute, int, 0444);
static int extra_mb;
module_param(extra_mb, int, 0444);

static struct xnu_image img;
static void *blob;

static void log_text(const char *s)
{
	fki_log(s, x_strlen(s));
}

static void log_hex(const char *name, u64_t value)
{
	char b[96];
	char *p = b;
	while (*name)
		*p++ = *name++;
	*p++ = ' '; *p++ = '0'; *p++ = 'x';
	for (int i = 60; i >= 0; i -= 4)
		*p++ = "0123456789abcdef"[(value >> i) & 15];
	*p = 0;
	log_text(b);
}

static int __init xnu_init_x86(void)
{
	const void *data;
	unsigned long size;
	int err;

	if (fki_version() != 0)
		return -EINVAL;
	err = fki_random_wait();
	if (err)
		return err;
	err = fki_blob_get(image, &data, &size, &blob);
	if (err) {
		log_hex("xnu-x86: cannot read image, err", (u64_t)err);
		return err;
	}

	/* Parse-only mode never allocates executable memory or runs foreign code. */
	unsigned long reserve = execute ? (unsigned long)(extra_mb > 0 ? extra_mb : 0) << 20 : ~0UL;
	err = macho_load(&img, data, size, reserve);
	if (err) {
		log_hex("xnu-x86: Mach-O load failed, err", (u64_t)err);
		fki_blob_put(blob);
		blob = 0;
		return err;
	}
	log_hex("xnu-x86: image bytes", size);
	log_hex("xnu-x86: unslid image base", img.vmlow);
	log_hex("xnu-x86: image size", img.vmhigh - img.vmlow);
	log_hex("xnu-x86: entry", img.entry);
	if (!execute) {
		log_text("xnu-x86: parse-only check passed; no code was entered");
		return 0;
	}
	if (!img.entry || !img.region.va || img.entry < img.vmlow || img.entry >= img.vmhigh) {
		log_text("xnu-x86: refusing invalid entry point");
		return -ENOEXEC;
	}
	log_text("xnu-x86: entering explicit smoke-test entry (unsafe for production images)");
	long ret = xnu_world_call_x86(img.entry + img.slide, 0);
	log_hex("xnu-x86: entry returned", (u64_t)ret);
	return 0;
}

static void __exit xnu_exit_x86(void)
{
	if (img.region.va)
		fki_region_free(&img.region);
	if (blob)
		fki_blob_put(blob);
}

module_init(xnu_init_x86);
module_exit(xnu_exit_x86);
MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION("XNU AMD64 Mach-O bring-up loader");
MODULE_SOFTDEP("pre: fki");
