// SPDX-License-Identifier: GPL-2.0
/*
 * Provide kernel BTF information for introspection and use by eBPF tools.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/sysfs.h>

/* See scripts/link-vmlinux.sh, gen_btf() func for details */
extern char __weak __start_BTF[];
extern char __weak __stop_BTF[];

static ssize_t
btf_vmlinux_read(struct file *file, struct kobject *kobj,
		 struct bin_attribute *bin_attr,
		 char *buf, loff_t off, size_t len)
{
	size_t size = bin_attr->size;

	/*
	 * Defense-in-depth bounds check.
	 *
	 * In newer kernels (5.x+), kernfs_file_direct_read() clamps *ppos
	 * against bin_attr->size before calling this callback, so an explicit
	 * check here is redundant. This is NOT true in 4.19 - the 4.19
	 * kernfs read path only clamps `len = min_t(size_t, count, PAGE_SIZE)`
	 * and passes raw *ppos to ops->read() with no bounds check against
	 * the file's declared size. Combined with .llseek = generic_file_llseek
	 * allowing arbitrary SEEK_SET, an unprivileged process can:
	 *   fd = open("/sys/kernel/btf/vmlinux", O_RDONLY);   // mode 0444
	 *   lseek(fd, 100 * 1024 * 1024, SEEK_SET);            // succeeds
	 *   read(fd, buf, 4096);                                // memcpy from
	 *                                                       // __start_BTF + 100MB
	 *
	 * The actual BTF blob is only a few MB, so the memcpy reads past it
	 * into arbitrary kernel memory, then copy_to_user()s it back to
	 * userspace. Best case: kernel memory info leak. Worst case: hits
	 * an unmapped page and panics - a trivially-triggered unprivileged
	 * DoS.
	 *
	 * bin_attr->size is populated in btf_vmlinux_init() before the file
	 * is registered with sysfs, so it's safe to read here.
	 */
	if (off >= size)
		return 0;
	if (len > size - off)
		len = size - off;

	memcpy(buf, __start_BTF + off, len);
	return len;
}

static struct bin_attribute bin_attr_btf_vmlinux __ro_after_init = {
	.attr = { .name = "vmlinux", .mode = 0444, },
	.read = btf_vmlinux_read,
};

struct kobject *btf_kobj;

static int __init btf_vmlinux_init(void)
{
	bin_attr_btf_vmlinux.size = __stop_BTF - __start_BTF;

	if (!__start_BTF || bin_attr_btf_vmlinux.size == 0)
		return 0;

	btf_kobj = kobject_create_and_add("btf", kernel_kobj);
	if (!btf_kobj)
		return -ENOMEM;

	return sysfs_create_bin_file(btf_kobj, &bin_attr_btf_vmlinux);
}

subsys_initcall(btf_vmlinux_init);
