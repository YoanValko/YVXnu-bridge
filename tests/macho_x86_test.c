// SPDX-License-Identifier: APSL-2.0
#include <stdio.h>
#include <string.h>
#include "../xnu-ko/macho.h"

int fki_region_alloc(unsigned long size, struct fki_region *r)
{
	(void)size; (void)r; return -12;
}
void fki_region_free(struct fki_region *r) { (void)r; }

static int check(int condition, const char *what)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", what);
		return 1;
	}
	return 0;
}

int main(void)
{
	unsigned char buf[512];
	struct mh64 *mh = (void *)buf;
	struct seg64 *seg;
	struct lc *thread;
	struct xnu_image img;
	memset(buf, 0, sizeof(buf));
	mh->magic = MH_MAGIC_64;
	mh->cputype = CPU_TYPE_X86_64;
	mh->filetype = 2; /* MH_EXECUTE */
	mh->ncmds = 2;
	mh->sizeofcmds = sizeof(*seg) + 184;
	seg = (void *)(mh + 1);
	seg->cmd = LC_SEGMENT_64;
	seg->cmdsize = sizeof(*seg);
	memcpy(seg->segname, "__TEXT", 6);
	seg->vmaddr = 0x100000;
	seg->vmsize = 0x1000;
	seg->fileoff = 0;
	seg->filesize = 0x100;
	thread = (void *)((unsigned char *)seg + sizeof(*seg));
	thread->cmd = LC_UNIXTHREAD;
	thread->cmdsize = 184;
	*(unsigned long long *)((unsigned char *)thread + 8 + 8 + 16 * 8) = 0x100080;

	int failures = 0;
	failures += check(macho_load(&img, buf, sizeof(buf), ~0UL) == 0, "valid x86-64 image");
	failures += check(img.vmlow == 0x100000 && img.vmhigh == 0x101000, "segment range");
	failures += check(img.entry == 0x100080, "UNIXTHREAD entry");
	thread->cmdsize = 0;
	failures += check(macho_load(&img, buf, sizeof(buf), ~0UL) < 0, "reject zero command size");
	thread->cmdsize = 184;
	mh->cputype = CPU_TYPE_ARM64;
	failures += check(macho_load(&img, buf, sizeof(buf), ~0UL) < 0, "reject foreign architecture");
	puts(failures ? "macho_x86_test: FAILED" : "macho_x86_test: PASS");
	return failures ? 1 : 0;
}
