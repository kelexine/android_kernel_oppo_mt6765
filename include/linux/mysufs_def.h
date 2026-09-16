/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Author:  kelexine <https://github.com/kelexine>
 * Date:    2026-09-16
 * Purpose: MYSUFS (SU File System for MySU) core definitions & constants
 */
#ifndef MYSU_MYSUFS_DEF_H
#define MYSU_MYSUFS_DEF_H

#include <linux/bits.h>
#include <linux/string.h>
#include <linux/version.h> // We need check kernel version.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
#include <linux/cred.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
#define d_in_lookup(dentry) (0)
#define d_lookup_done(dentry) do {} while (0)
#endif // to support 4.4 and older kernel

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
#define GFP_KERNEL_ACCOUNT GFP_KERNEL
#endif // to support 4.4 and older kernel

/********/
/* ENUM */
/********/
/* shared with userspace ksu_susfs tool */
#define MYSUFS_MAGIC 0xFAFAFAFA
#define CMD_MYSUFS_ADD_SUS_PATH 0x55550
#define CMD_MYSUFS_SET_ANDROID_DATA_ROOT_PATH 0x55551 /* deprecated */
#define CMD_MYSUFS_SET_SDCARD_ROOT_PATH 0x55552 /* deprecated */
#define CMD_MYSUFS_ADD_SUS_PATH_LOOP 0x55553
#define CMD_MYSUFS_ADD_SUS_MOUNT 0x55560 /* deprecated */
#define CMD_MYSUFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS 0x55561
#define CMD_MYSUFS_UMOUNT_FOR_ZYGOTE_ISO_SERVICE 0x55562 /* deprecated */
#define CMD_MYSUFS_ADD_SUS_KSTAT 0x55570
#define CMD_MYSUFS_UPDATE_SUS_KSTAT 0x55571
#define CMD_MYSUFS_ADD_SUS_KSTAT_STATICALLY 0x55572
#define CMD_MYSUFS_ADD_TRY_UMOUNT 0x55580 /* deprecated */
#define CMD_MYSUFS_SET_UNAME 0x55590
#define CMD_MYSUFS_ENABLE_LOG 0x555a0
#define CMD_MYSUFS_SET_CMDLINE_OR_BOOTCONFIG 0x555b0
#define CMD_MYSUFS_ADD_OPEN_REDIRECT 0x555c0
#define CMD_MYSUFS_SHOW_VERSION 0x555e1
#define CMD_MYSUFS_SHOW_ENABLED_FEATURES 0x555e2
#define CMD_MYSUFS_SHOW_VARIANT 0x555e3
#define CMD_MYSUFS_SHOW_SUS_SU_WORKING_MODE 0x555e4 /* deprecated */
#define CMD_MYSUFS_IS_SUS_SU_READY 0x555f0 /* deprecated */
#define CMD_MYSUFS_SUS_SU 0x60000 /* deprecated */
#define CMD_MYSUFS_ENABLE_AVC_LOG_SPOOFING 0x60010
#define CMD_MYSUFS_ADD_SUS_MAP 0x60020

#define MYSUFS_MAX_LEN_PATHNAME 256 // 256 should address many paths already unless you are doing some strange experimental stuff, then set your own desired length
#define MYSUFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE 8192 // 8192 is enough I guess
#define MYSUFS_ENABLED_FEATURES_SIZE 8192 // 8192 is enough I guess
#define MYSUFS_MAX_VERSION_BUFSIZE 16
#define MYSUFS_MAX_VARIANT_BUFSIZE 16

#define TRY_UMOUNT_DEFAULT 0 /* used by mysufs_try_umount() */
#define TRY_UMOUNT_DETACH 1 /* used by mysufs_try_umount() */

#define DEFAULT_MYSU_MNT_ID 2000000000 /* used for mounts created or single cloned by ksu process */
#define DEFAULT_MYSU_MNT_GROUP_ID 200000 /* used by mount->mnt_group_id */
#define DEFAULT_MYSU_MNT_MINOR_DEV (1 << 12) /* should be way enough, here minor(dev) begins with 4097 */

#ifndef FUSE_SUPER_MAGIC
#define FUSE_SUPER_MAGIC 0x65735546
#endif

/*
 * mnt->mnt.mnt_flags => An 'int' primitive storing flag 'VFSMOUNT_MNT_FLAGS_'
 * task_struct->thread_info.flags => storing flag 'TIF_' which is an unsigned long primitive :D
 * inode->i_state => A 'unsigned long' type storing flag 'AS_FLAGS_', bit 1 to 31 is not usable since 6.12
 * nd->state => storing flag 'ND_STATE_'
 * nd->flags => storing flag 'ND_FLAGS_'
 * statx request_mark => storing flag 'STATX_'
 */

#define VFSMOUNT_MNT_FLAGS_MYSU_UNSHARED_MNT 0x80000000 /* used for mounts that are unshared by ksu process */

#define TIF_PROC_UMOUNTED 33
#define TIF_PROC_NO_SU 34
#define TIF_PROC_UMOUNTED_FOR_ZYGOTE_NEXT 35

#define AS_FLAGS_SUS_PATH 33
#define AS_FLAGS_SUS_MOUNT 34
#define AS_FLAGS_SUS_KSTAT 35
#define AS_FLAGS_OPEN_REDIRECT 36
#define AS_FLAGS_SUS_MAP 39

#define ND_STATE_LOOKUP_LAST 32
#define ND_STATE_OPEN_LAST 64
#define ND_FLAGS_LOOKUP_LAST 0x2000000

#define STATX_SUS_KSTAT 0x10000000U
#define STATX_SUS_KSTAT_FUSE 0x20000000U

static inline bool mysufs_starts_with(const char *str, const char *prefix) {
	while (*prefix) {
		if (*str++ != *prefix++)
			return false;
	}
	return true;
}

static inline bool mysufs_ends_with(const char *str, const char *suffix) {
	size_t str_len, suffix_len;

	if (!str || !suffix)
		return false;

	str_len = strlen(str);
	suffix_len = strlen(suffix);

	if (suffix_len > str_len)
		return false;

	return !strcmp(str + str_len - suffix_len, suffix);
}

/* From KernelSU */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
typedef const struct qstr *mysufs_fname_t;
#define mysufs_fname_len(f) ((f)->len)
#define mysufs_fname_arg(f) ((f)->name)
#else
typedef const unsigned char *mysufs_fname_t;
#define mysufs_fname_len(f) (strlen(f))
#define mysufs_fname_arg(f) (f)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
#define MYSUFS_DECL_FSNOTIFY_OPS(name)                                            \
int name(struct fsnotify_mark *mark, u32 mask, struct inode *inode,    \
struct inode *dir, const struct qstr *file_name, u32 cookie)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
#define MYSUFS_DECL_FSNOTIFY_OPS(name)                                            \
int name(struct fsnotify_group *group, struct inode *inode, u32 mask,  \
const void *data, int data_type, mysufs_fname_t file_name,       \
u32 cookie, struct fsnotify_iter_info *iter_info)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
#define MYSUFS_DECL_FSNOTIFY_OPS(name)                                            \
int name(struct fsnotify_group *group, struct inode *inode, u32 mask,  \
const void *data, int data_type, mysufs_fname_t file_name,       \
u32 cookie, struct fsnotify_iter_info *iter_info)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
#define MYSUFS_DECL_FSNOTIFY_OPS(name)                                            \
int name(struct fsnotify_group *group, struct inode *inode,            \
struct fsnotify_mark *inode_mark,                             \
struct fsnotify_mark *vfsmount_mark, u32 mask,                \
const void *data, int data_type, mysufs_fname_t file_name,       \
u32 cookie, struct fsnotify_iter_info *iter_info)
#else
#define MYSUFS_DECL_FSNOTIFY_OPS(name)                                            \
int name(struct fsnotify_group *group, struct inode *inode,            \
struct fsnotify_mark *inode_mark,                             \
struct fsnotify_mark *vfsmount_mark, u32 mask, void *data,    \
int data_type, mysufs_fname_t file_name, u32 cookie)
#endif

static inline bool mysufs_is_current_app_uid(void) {
	return ((current_uid().val % 100000) >= 10000);
}

static inline bool mysufs_is_current_proc_umounted(void) {
	return (likely(test_thread_flag(TIF_PROC_UMOUNTED)));
}

static inline void mysufs_set_current_proc_umounted(void) {
	set_thread_flag(TIF_PROC_UMOUNTED);
}

static inline void mysufs_clear_current_proc_umounted(void) {
	clear_thread_flag(TIF_PROC_UMOUNTED);
}

static inline bool mysufs_is_current_proc_umounted_for_zygote_next(void) {
	return (likely(test_thread_flag(TIF_PROC_UMOUNTED_FOR_ZYGOTE_NEXT)));
}

static inline void mysufs_set_current_proc_umounted_for_zygote_next(void) {
	set_thread_flag(TIF_PROC_UMOUNTED_FOR_ZYGOTE_NEXT);
}

static inline void mysufs_clear_current_proc_umounted_for_zygote_next(void) {
	clear_thread_flag(TIF_PROC_UMOUNTED_FOR_ZYGOTE_NEXT);
}

static inline bool mysufs_is_current_proc_umounted_app(void) {
	return (likely(test_thread_flag(TIF_PROC_UMOUNTED)) &&
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
			__kuid_val(current_uid()) >= 10000);
#else
			current_uid().val >= 10000);
#endif
}

static inline bool mysufs_is_current_proc_no_su(void) {
	return (likely(test_thread_flag(TIF_PROC_NO_SU)));
}

static inline void mysufs_set_current_proc_no_su(void) {
	set_thread_flag(TIF_PROC_NO_SU);
}

static inline void mysufs_clear_current_proc_no_su(void) {
	clear_thread_flag(TIF_PROC_NO_SU);
}

#define MYSUFS_IS_INODE_SUS_MAP(inode) \
		inode && inode->i_mapping && \
		unlikely(test_bit(AS_FLAGS_SUS_MAP, &inode->i_state)) && \
		mysufs_is_current_proc_umounted_app()

#define MYSUFS_IS_INODE_OPEN_REDIRECT_WITHOUT_UID_CHECK(inode) \
		inode && inode->i_mapping && \
		unlikely(test_bit(AS_FLAGS_OPEN_REDIRECT, &inode->i_state))

#define MYSUFS_IS_INODE_OPEN_REDIRECT(inode) \
		inode && inode->i_mapping && \
		unlikely(test_bit(AS_FLAGS_OPEN_REDIRECT, &inode->i_state)) && \
		mysufs_is_current_proc_umounted_app()

extern bool is_mysu_domain(void);

static inline bool mysufs_is_current_mysu_domain(void) {
	return is_mysu_domain();
}

#endif // #ifndef MYSU_MYSUFS_DEF_H
