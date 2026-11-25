// SPDX-License-Identifier: GPL-2.0-only
/*
 * x86-64 bootstrap implementation of FKI.
 *
 * This deliberately provides only host services that do not require taking
 * over the CPU's page tables or exception machinery.  The ARM implementation
 * cannot be reused for those operations: x86 has different CR3, TLB, timer,
 * and exception ABIs.  Keeping this as a separate translation unit prevents
 * accidental use of ARM system-register operations on an AMD64 host.
 */
#include "fki.h"
#include "fki_internal.h"
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/gfp.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <asm/ptrace.h>

#define FKI_ALT_STACK_ORDER get_order(2 * THREAD_SIZE)
static void *fki_alt_stack[NR_CPUS];
extern void fki_run_on_stack_x86(void (*fn)(void *), void *arg, void *top);

static int fki_stack_init(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		fki_alt_stack[cpu] = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, FKI_ALT_STACK_ORDER);
		if (!fki_alt_stack[cpu])
			goto fail;
	}
	return 0;
fail:
	for_each_possible_cpu(cpu) {
		if (fki_alt_stack[cpu])
			free_pages((unsigned long)fki_alt_stack[cpu], FKI_ALT_STACK_ORDER);
		fki_alt_stack[cpu] = NULL;
	}
	return -ENOMEM;
}

static void fki_stack_exit(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		if (fki_alt_stack[cpu])
			free_pages((unsigned long)fki_alt_stack[cpu], FKI_ALT_STACK_ORDER);
		fki_alt_stack[cpu] = NULL;
	}
}

int fki_version(void) { return FKI_VERSION; }
EXPORT_SYMBOL(fki_version);

void fki_log(const char *s, unsigned long len) { pr_info("fki: %.*s\n", (int)len, s); }
EXPORT_SYMBOL(fki_log);
void fki_log_alert(const char *s, unsigned long len) { pr_alert("fki: %.*s\n", (int)len, s); }
EXPORT_SYMBOL(fki_log_alert);

struct x86_region_handle {
	struct page *first;
	unsigned long order;
	struct page **pages;
};

int fki_region_alloc(unsigned long size, struct fki_region *r)
{
	unsigned long i, n, order, bytes;
	struct page *first;
	struct page **pages;
	struct x86_region_handle *handle;
	void *va;

	if (!size || size > ULONG_MAX - PAGE_SIZE + 1)
		return -EINVAL;
	bytes = PAGE_ALIGN(size);
	order = get_order(bytes);
	if (order >= BITS_PER_LONG)
		return -E2BIG;
	first = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (!first)
		return -ENOMEM;
	n = 1UL << order;
	pages = kvmalloc_array(n, sizeof(*pages), GFP_KERNEL);
	if (!pages)
		goto free_block;
	for (i = 0; i < n; i++) {
		pages[i] = first + i;
	}
	va = vmap(pages, n, VM_MAP, PAGE_KERNEL_EXEC);
	if (!va)
		goto free_pages_array;
	handle = kmalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		goto unmap;
	handle->first = first;
	handle->order = order;
	handle->pages = pages;
	r->va = va;
	r->pa = page_to_phys(first);
	r->size = n << PAGE_SHIFT;
	r->handle = handle;
	return 0;
unmap:
	vunmap(va);
free_pages_array:
	kvfree(pages);
free_block:
	__free_pages(first, order);
	return -ENOMEM;
}
EXPORT_SYMBOL(fki_region_alloc);
void fki_region_free(struct fki_region *r)
{
	if (!r || !r->handle)
		return;
	struct x86_region_handle *handle = r->handle;

	vunmap(r->va);
	__free_pages(handle->first, handle->order);
	kvfree(handle->pages);
	kfree(handle);
	r->va = NULL;
	r->pa = 0;
	r->size = 0;
	r->handle = NULL;
}
EXPORT_SYMBOL(fki_region_free);

int fki_blob_get(const char *name, const void **data, unsigned long *size, void **handle)
{
	const struct firmware *fw;
	int err = request_firmware(&fw, name, NULL);
	if (err)
		return err;
	*data = fw->data; *size = fw->size; *handle = (void *)fw;
	return 0;
}
EXPORT_SYMBOL(fki_blob_get);
void fki_blob_put(void *handle) { release_firmware(handle); }
EXPORT_SYMBOL(fki_blob_put);

/* x86 has no ARM pointer-authentication key state to swap. */
unsigned long fki_enter(const struct fki_pac_keys *foreign, struct fki_pac_keys *saved)
{
	unsigned long flags;
	memset(saved, 0, sizeof(*saved));
	preempt_disable();
	local_irq_save(flags);
	return flags;
}
EXPORT_SYMBOL(fki_enter);
void fki_leave(unsigned long cookie, const struct fki_pac_keys *saved)
{
	local_irq_restore(cookie);
	preempt_enable();
}
EXPORT_SYMBOL(fki_leave);
void fki_pac_swap(const struct fki_pac_keys *load, struct fki_pac_keys *save)
{
	memset(save, 0, sizeof(*save));
}
EXPORT_SYMBOL(fki_pac_swap);

/* nanoseconds are an invariant unit; the x86 XNU layer must consume this unit. */
fki_u64 fki_counter_frequency(void) { return NSEC_PER_SEC; }
EXPORT_SYMBOL(fki_counter_frequency);
fki_u64 fki_counter(void) { return ktime_get_ns(); }
EXPORT_SYMBOL(fki_counter);
void fki_random(void *buf, unsigned long len) { get_random_bytes(buf, len); }
EXPORT_SYMBOL(fki_random);
int fki_random_wait(void) { return wait_for_random_bytes(); }
EXPORT_SYMBOL(fki_random_wait);

int fki_bind_cpu(int cpu)
{
	if (cpu < 0 || cpu >= nr_cpu_ids || !cpu_online(cpu))
		return -EINVAL;
	return set_cpus_allowed_ptr(current, cpumask_of(cpu));
}
EXPORT_SYMBOL(fki_bind_cpu);
int fki_num_cpus(void) { return num_online_cpus(); }
EXPORT_SYMBOL(fki_num_cpus);
int fki_cpu_mpidr(int cpu, fki_u64 *mpidr)
{
	if (cpu < 0 || cpu >= nr_cpu_ids || !cpu_online(cpu))
		return -EINVAL;
	/* An x86 logical CPU number replaces ARM's MPIDR in the x86 ABI. */
	*mpidr = cpu;
	return 0;
}
EXPORT_SYMBOL(fki_cpu_mpidr);

void *fki_thread_start(int (*fn)(void *), void *arg, int cpu, const char *name)
{
	struct task_struct *task;
	if (cpu < 0 || cpu >= nr_cpu_ids || !cpu_online(cpu))
		return NULL;
	task = kthread_create(fn, arg, "%s", name);
	if (IS_ERR(task))
		return NULL;
	kthread_bind(task, cpu);
	get_task_struct(task);
	wake_up_process(task);
	return task;
}
EXPORT_SYMBOL(fki_thread_start);
void fki_thread_stop(void *thread)
{
	if (thread) {
		kthread_stop(thread);
		put_task_struct(thread);
	}
}
EXPORT_SYMBOL(fki_thread_stop);
int fki_thread_should_stop(void) { return kthread_should_stop(); }
EXPORT_SYMBOL(fki_thread_should_stop);
void fki_thread_wait(const volatile unsigned long *flag)
{
	set_current_state(TASK_INTERRUPTIBLE);
	while (!READ_ONCE(*flag) && !kthread_should_stop()) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);
}
EXPORT_SYMBOL(fki_thread_wait);
void fki_thread_wake(void *thread)
{
	if (thread)
		wake_up_process(thread);
}
EXPORT_SYMBOL(fki_thread_wake);
void fki_msleep(unsigned int ms) { msleep(ms); }
EXPORT_SYMBOL(fki_msleep);
static void fki_kick_noop(void *unused) { }
void fki_kick_cpu(int cpu)
{
	if (cpu >= 0 && cpu < nr_cpu_ids && cpu != smp_processor_id())
		smp_call_function_single(cpu, fki_kick_noop, NULL, 0);
}
EXPORT_SYMBOL(fki_kick_cpu);
void fki_realtime(fki_u64 *sec, fki_u64 *nsec)
{
	struct timespec64 ts;
	ktime_get_real_ts64(&ts); *sec = ts.tv_sec; *nsec = ts.tv_nsec;
}
EXPORT_SYMBOL(fki_realtime);

/* A small Linux netdev exposes the same packet ABI as the ARM backend. */
#define FKI_NET_QLEN 1024
static struct net_device *fki_netdev;
static struct sk_buff_head fki_net_rxq;
static netdev_tx_t fki_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	if (skb_queue_len(&fki_net_rxq) >= FKI_NET_QLEN) {
		dev->stats.tx_dropped++;
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	skb_queue_tail(&fki_net_rxq, skb);
	return NETDEV_TX_OK;
}
static int fki_net_up(struct net_device *dev) { netif_start_queue(dev); return 0; }
static int fki_net_down(struct net_device *dev) { netif_stop_queue(dev); return 0; }
static const struct net_device_ops fki_net_ops = {
	.ndo_open = fki_net_up,
	.ndo_stop = fki_net_down,
	.ndo_start_xmit = fki_net_xmit,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
};
struct fki_net_io { const void *in; void *out; unsigned long len; long result; };
struct fki_net_send_args { const void *frame; unsigned long len; long result; };
static void fki_net_send_body(void *arg)
{
	struct fki_net_send_args *a = arg;
	struct sk_buff *skb;
	local_irq_enable();
	preempt_enable();
	if (!fki_netdev || a->len < ETH_HLEN || a->len > ETH_FRAME_LEN + 4) {
		a->result = -ENODEV;
		goto out;
	}
	skb = netdev_alloc_skb(fki_netdev, a->len + NET_IP_ALIGN);
	if (!skb) {
		a->result = -ENOMEM;
		goto out;
	}
	skb_reserve(skb, NET_IP_ALIGN);
	skb_put_data(skb, a->frame, a->len);
	skb->protocol = eth_type_trans(skb, fki_netdev);
	fki_netdev->stats.rx_packets++;
	fki_netdev->stats.rx_bytes += a->len;
	netif_rx(skb);
	a->result = a->len;
out:
	preempt_disable();
	local_irq_disable();
}
long fki_net_send(const void *frame, unsigned long len)
{
	struct fki_net_send_args a = { .frame = frame, .len = len, .result = -EIO };
	fki_linux_runs_ext(fki_net_send_body, &a);
	return a.result;
}
EXPORT_SYMBOL(fki_net_send);
long fki_net_recv(void *buf, unsigned long len)
{
	struct sk_buff *skb = skb_dequeue(&fki_net_rxq);
	long result = 0;
	if (!skb)
		return 0;
	if (skb->len > len)
		result = -EMSGSIZE;
	else if (skb_copy_bits(skb, 0, buf, skb->len) == 0)
		result = skb->len;
	else
		result = -EIO;
	dev_consume_skb_any(skb);
	return result;
}
EXPORT_SYMBOL(fki_net_recv);
long fki_net_pending(void) { return skb_queue_len(&fki_net_rxq); }
EXPORT_SYMBOL(fki_net_pending);

/* The allocator above already returns a physically contiguous power-of-two
 * block; expose it through the foreign-DRAM ABI as well. */
int fki_contig_alloc(unsigned long size, struct fki_region *r)
{
	return fki_region_alloc(size, r);
}
EXPORT_SYMBOL(fki_contig_alloc);
void fki_contig_free(struct fki_region *r)
{
	fki_region_free(r);
}
EXPORT_SYMBOL(fki_contig_free);
/* The debug peek is useful on x86 too: Linux's direct map covers ordinary
 * RAM, while invalid or I/O PFNs are rejected rather than dereferenced. */
int fki_window_l1(unsigned long va, unsigned long long **l1_va, fki_u64 *l1_pa) { return -EOPNOTSUPP; }
EXPORT_SYMBOL(fki_window_l1);
void fki_window_set(unsigned long long *slot, fki_u64 desc) { }
EXPORT_SYMBOL(fki_window_set);

struct fki_idle_args {
	fki_u64 deadline, max_ns;
	const volatile unsigned int *wake;
};
static void fki_idle_body(void *arg)
{
	struct fki_idle_args *a = arg;
	fki_u64 now, wait_ns;
	unsigned long usec;

	if (a->wake && READ_ONCE(*a->wake))
		return;
	now = ktime_get_ns();
	wait_ns = a->deadline > now ? a->deadline - now : 0;
	if (wait_ns > a->max_ns)
		wait_ns = a->max_ns;
	if (!wait_ns) {
		local_irq_enable();
		preempt_enable();
		cond_resched();
		preempt_disable();
		local_irq_disable();
		return;
	}
	usec = max_t(fki_u64, 2, min_t(fki_u64, wait_ns / NSEC_PER_USEC, 1000000));
	local_irq_enable();
	preempt_enable();
	usleep_range(1, usec);
	preempt_disable();
	local_irq_disable();
}

void fki_idle_until(fki_u64 deadline, fki_u64 max_ns, const volatile unsigned int *wake)
{
	struct fki_idle_args a = { .deadline = deadline, .max_ns = max_ns, .wake = wake };
	fki_linux_runs_ext(fki_idle_body, &a);
}
EXPORT_SYMBOL(fki_idle_until);

static void fki_yield_body(void *unused)
{
	local_irq_enable();
	preempt_enable();
	cond_resched();
	preempt_disable();
	local_irq_disable();
}

void fki_yield(void)
{
	fki_linux_runs_ext(fki_yield_body, NULL);
}
EXPORT_SYMBOL(fki_yield);

struct fki_file_args {
	struct file *file;
	int write;
	void *buf;
	unsigned long len;
	fki_u64 off;
	long result;
};
static void fki_file_rw_body(void *arg)
{
	struct fki_file_args *a = arg;
	loff_t pos = a->off;
	ssize_t r;

	local_irq_enable();
	preempt_enable();
	r = a->write ? kernel_write(a->file, a->buf, a->len, &pos) : kernel_read(a->file, a->buf, a->len, &pos);
	preempt_disable();
	local_irq_disable();
	a->result = r;
}

void *fki_file_open(const char *path, int writable, fki_u64 *size)
{
	struct file *file = filp_open(path, (writable ? O_RDWR : O_RDONLY) | O_LARGEFILE, 0);
	if (IS_ERR(file))
		return NULL;
	if (size)
		*size = i_size_read(file->f_mapping->host);
	return file;
}
EXPORT_SYMBOL(fki_file_open);
long fki_file_rw(void *handle, int write, void *buf, unsigned long len, fki_u64 off)
{
	struct file *file = handle;
	struct fki_file_args args = {
		.file = file, .write = write, .buf = buf, .len = len, .off = off, .result = -EIO,
	};
	if (!file)
		return -EBADF;
	fki_linux_runs_ext(fki_file_rw_body, &args);
	return args.result;
}
EXPORT_SYMBOL(fki_file_rw);
long fki_file_pwrite(void *handle, const void *buf, unsigned long len, fki_u64 off)
{
	struct file *file = handle;
	loff_t pos = off;
	if (!file)
		return -EBADF;
	return kernel_write(file, buf, len, &pos);
}
EXPORT_SYMBOL(fki_file_pwrite);
void fki_file_close(void *handle)
{
	if (handle)
		filp_close(handle, NULL);
}
EXPORT_SYMBOL(fki_file_close);

/* x86-64 register view used by the architecture-neutral fault callback. */
unsigned long fki_regs_pc(void *regs)
{
	return regs ? instruction_pointer((struct pt_regs *)regs) : 0;
}
EXPORT_SYMBOL(fki_regs_pc);
unsigned long fki_regs_lr(void *regs)
{
	/* There is no link register on x86; the interrupted stack pointer is the
	 * closest useful diagnostic analogue and is never used as a resume PC. */
	return regs ? ((struct pt_regs *)regs)->sp : 0;
}
EXPORT_SYMBOL(fki_regs_lr);
unsigned long fki_regs_pstate(void *regs)
{
	return regs ? ((struct pt_regs *)regs)->flags : 0;
}
EXPORT_SYMBOL(fki_regs_pstate);
void fki_regs_set_pc(void *regs, unsigned long pc)
{
	if (regs)
		instruction_pointer_set((struct pt_regs *)regs, pc);
}
EXPORT_SYMBOL(fki_regs_set_pc);
void fki_regs_set_reg(void *regs, unsigned int n, unsigned long v)
{
	struct pt_regs *r = regs;
	if (!r)
		return;
	switch (n) {
	case 0: r->ax = v; break;
	case 1: r->cx = v; break;
	case 2: r->dx = v; break;
	case 3: r->bx = v; break;
	case 4: r->bp = v; break;
	case 5: r->si = v; break;
	case 6: r->di = v; break;
	case 7: r->r8 = v; break;
	case 8: r->r9 = v; break;
	case 9: r->r10 = v; break;
	case 10: r->r11 = v; break;
	case 11: r->r12 = v; break;
	case 12: r->r13 = v; break;
	case 13: r->r14 = v; break;
	case 14: r->r15 = v; break;
	default: break;
	}
}
EXPORT_SYMBOL(fki_regs_set_reg);
unsigned long fki_regs_reg(void *regs, unsigned int n)
{
	struct pt_regs *r = regs;
	if (!r)
		return 0;
	switch (n) {
	case 0: return r->ax;
	case 1: return r->cx;
	case 2: return r->dx;
	case 3: return r->bx;
	case 4: return r->bp;
	case 5: return r->si;
	case 6: return r->di;
	case 7: return r->r8;
	case 8: return r->r9;
	case 9: return r->r10;
	case 10: return r->r11;
	case 11: return r->r12;
	case 12: return r->r13;
	case 13: return r->r14;
	case 14: return r->r15;
	default: return 0;
	}
}
EXPORT_SYMBOL(fki_regs_reg);
int fki_fault_register(unsigned long lo, unsigned long hi, int (*handler)(unsigned long, unsigned long, void *)) { return -EOPNOTSUPP; }
EXPORT_SYMBOL(fki_fault_register);
void fki_fault_unregister(void) { }
EXPORT_SYMBOL(fki_fault_unregister);
fki_u64 fki_phys_peek(fki_u64 pa)
{
	if (!pfn_valid(PHYS_PFN(pa)))
		return ~0ULL;
	return READ_ONCE(*(u64 *)phys_to_virt(pa & ~7ULL));
}
EXPORT_SYMBOL(fki_phys_peek);

/* The hosted kernel's stack is not a Linux task stack.  Any callback that
 * enables scheduling must run on the per-CPU Linux-owned stack instead. */
void fki_linux_runs_ext(void (*fn)(void *), void *arg)
{
	void *stack = READ_ONCE(fki_alt_stack[smp_processor_id()]);
	if (stack)
		fki_run_on_stack_x86(fn, arg, (char *)stack + (PAGE_SIZE << FKI_ALT_STACK_ORDER));
	else
		pr_warn_ratelimited("fki: alternate Linux stack unavailable; refusing scheduled callback\n");
}

static int __init fki_init(void)
{
	int err;
	err = fki_stack_init();
	if (err)
		return err;
	skb_queue_head_init(&fki_net_rxq);
	err = fki_hostcall_init();
	if (err)
		pr_warn("fki: hostcall device unavailable (%d)\n", err);
	fki_netdev = alloc_etherdev(0);
	if (!fki_netdev)
		return 0;
	strscpy(fki_netdev->name, "xnu0", IFNAMSIZ);
	fki_netdev->netdev_ops = &fki_net_ops;
	eth_hw_addr_random(fki_netdev);
	fki_netdev->min_mtu = ETH_MIN_MTU;
	fki_netdev->max_mtu = ETH_DATA_LEN;
	if (register_netdev(fki_netdev)) {
		free_netdev(fki_netdev);
		fki_netdev = NULL;
	}
	return 0;
}
static void __exit fki_exit(void)
{
	fki_hostcall_exit();
	if (fki_netdev) {
		unregister_netdev(fki_netdev);
		free_netdev(fki_netdev);
		fki_netdev = NULL;
	}
	skb_queue_purge(&fki_net_rxq);
	fki_stack_exit();
}
module_init(fki_init);
module_exit(fki_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Foreign kernel interface v0 (x86-64 bootstrap)");
