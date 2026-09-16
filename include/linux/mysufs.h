/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Author:  kelexine <https://github.com/kelexine>
 * Date:    2026-09-16
 * Purpose: MYSUFS (SU File System for MySU) core prototypes & data structures
 */
#ifndef MYSU_MYSUFS_H
#define MYSU_MYSUFS_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/hashtable.h>
#include <linux/path.h>
#include <linux/mysufs_def.h>
#include <linux/statfs.h>

#define MYSUFS_VERSION "v2.3.0"
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#define MYSUFS_VARIANT "NON-GKI"
#else
#define MYSUFS_VARIANT "GKI"
#endif

/*********/
/* MACRO */
/*********/
#define getname_safe(name) (name == NULL ? ERR_PTR(-EINVAL) : getname(name))
#define putname_safe(name) (IS_ERR(name) ? NULL : putname(name))

/********/
/* ENUM */
/********/
enum UID_SCHEME {
	UID_NON_APP_PROC = 0,
	UID_ROOT_PROC_EXCEPT_SU_PROC,
	UID_NON_SU_PROC,
	UID_UMOUNTED_APP_PROC,
	UID_UMOUNTED_PROC,
};

/**********/
/* STRUCT */
/**********/
#ifndef _MYSUFS_STRUCTS_DEFINED
#define _MYSUFS_STRUCTS_DEFINED

/* sus_path */
#ifdef CONFIG_MYSU_MYSUFS_SUS_PATH
struct st_mysufs_sus_path {
	char                                    target_pathname[MYSUFS_MAX_LEN_PATHNAME];
	int                                     err;
};
#endif

/* sus_mount */
#ifdef CONFIG_MYSU_MYSUFS_SUS_MOUNT
struct st_mysufs_hide_sus_mnts_for_non_su_procs {
	bool                                    enabled;
	int                                     err;
};
#endif // #ifdef CONFIG_MYSU_MYSUFS_SUS_MOUNT

/* sus_kstat */
#ifdef CONFIG_MYSU_MYSUFS_SUS_KSTAT
#ifndef KSTAT_SPOOF_INO
#define KSTAT_SPOOF_INO (1 << 0)
#define KSTAT_SPOOF_DEV (1 << 1)
#define KSTAT_SPOOF_NLINK (1 << 2)
#define KSTAT_SPOOF_SIZE (1 << 3)
#define KSTAT_SPOOF_ATIME_TV_SEC (1 << 4)
#define KSTAT_SPOOF_ATIME_TV_NSEC (1 << 5)
#define KSTAT_SPOOF_MTIME_TV_SEC (1 << 6)
#define KSTAT_SPOOF_MTIME_TV_NSEC (1 << 7)
#define KSTAT_SPOOF_CTIME_TV_SEC (1 << 8)
#define KSTAT_SPOOF_CTIME_TV_NSEC (1 << 9)
#define KSTAT_SPOOF_BLOCKS (1 << 10)
#define KSTAT_SPOOF_BLKSIZE (1 << 11)
#endif

struct st_mysufs_sus_kstat {
	int                                     is_statically;
	unsigned long                           target_ino;
	char                                    target_pathname[MYSUFS_MAX_LEN_PATHNAME];
	unsigned long                           spoofed_ino;
	unsigned long                           spoofed_dev;
	unsigned int                            spoofed_nlink;
	long long                               spoofed_size;
	long                                    spoofed_atime_tv_sec;
	unsigned long                           spoofed_atime_tv_nsec;
	long                                    spoofed_mtime_tv_sec;
	unsigned long                           spoofed_mtime_tv_nsec;
	long                                    spoofed_ctime_tv_sec;
	unsigned long                           spoofed_ctime_tv_nsec;
	long long                               spoofed_blocks;
	long                                    spoofed_blksize;
	int                                     flags;
	int                                     err;
};
#endif

/* spoof_uname */
#ifdef CONFIG_MYSU_MYSUFS_SPOOF_UNAME
struct st_mysufs_uname {
	char                                    release[__NEW_UTS_LEN+1];
	char                                    version[__NEW_UTS_LEN+1];
	int                                     err;
};
#endif

/* enable_log */
#ifdef CONFIG_MYSU_MYSUFS_ENABLE_LOG
struct st_mysufs_log {
	bool                                    enabled;
	int                                     err;
};
#endif

/* spoof_cmdline_or_bootconfig */
#ifdef CONFIG_MYSU_MYSUFS_SPOOF_CMDLINE_OR_BOOTCONFIG
struct st_mysufs_spoof_cmdline_or_bootconfig {
	char                                    fake_cmdline_or_bootconfig[MYSUFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE];
	int                                     err;
};
#endif

/* open_redirect */
#ifdef CONFIG_MYSU_MYSUFS_OPEN_REDIRECT
struct st_mysufs_open_redirect {
	char                                    target_pathname[MYSUFS_MAX_LEN_PATHNAME];
	char                                    redirected_pathname[MYSUFS_MAX_LEN_PATHNAME];
	int                                     uid_scheme;
	int                                     err;
};
#endif

/* sus_map */
#ifdef CONFIG_MYSU_MYSUFS_SUS_MAP
struct st_mysufs_sus_map {
	char                                    target_pathname[MYSUFS_MAX_LEN_PATHNAME];
	int                                     err;
};
#endif

/* avc log spoofing */
struct st_mysufs_avc_log_spoofing {
	bool                                    enabled;
	int                                     err;
};

/* get enabled features */
struct st_mysufs_enabled_features {
	char                                    enabled_features[MYSUFS_ENABLED_FEATURES_SIZE];
	int                                     err;
};

/* show variant */
struct st_mysufs_variant {
	char                                    mysufs_variant[16];
	int                                     err;
};

/* show version */
struct st_mysufs_version {
	char                                    mysufs_version[16];
	int                                     err;
};

#endif /* _MYSUFS_STRUCTS_DEFINED */

/* Kernel-internal list and hash nodes */
#ifdef CONFIG_MYSU_MYSUFS_SUS_PATH
struct st_mysufs_sus_path_list {
	struct list_head                        list;
	struct st_mysufs_sus_path               info;
	char                                    target_pathname[MYSUFS_MAX_LEN_PATHNAME];
};
#endif

#ifdef CONFIG_MYSU_MYSUFS_SUS_KSTAT
struct st_mysufs_sus_kstat_hlist {
	struct hlist_node                       node;
	unsigned long                           target_ino;
	unsigned long                           target_dev;
	struct kstatfs                          spoofed_kstatfs;
	int                                     spoofed_mnt_id;
	bool                                    is_fuse;
	struct st_mysufs_sus_kstat              info;
};
#endif

#ifdef CONFIG_MYSU_MYSUFS_OPEN_REDIRECT
struct st_mysufs_open_redirect_hlist {
	struct hlist_node                       node;
	unsigned long                           target_ino;
	unsigned long                           target_dev;
	unsigned long                           redirected_ino;
	unsigned long                           redirected_dev;
	int                                     spoofed_mnt_id;
	struct kstatfs                          spoofed_kstatfs;
	struct st_mysufs_open_redirect          info;
	bool                                    reversed_lookup_only;
};
#endif

/***********************/
/* FORWARD DECLARATION */
/***********************/
/* sus_path */
#ifdef CONFIG_MYSU_MYSUFS_SUS_PATH
void mysufs_add_sus_path(void __user **user_info);
void mysufs_add_sus_path_loop(void __user **user_info);
#endif

/* sus_mount */
#ifdef CONFIG_MYSU_MYSUFS_SUS_MOUNT
void mysufs_set_hide_sus_mnts_for_non_su_procs(void __user **user_info);
#endif // #ifdef CONFIG_MYSU_MYSUFS_SUS_MOUNT

/* sus_kstat */
#ifdef CONFIG_MYSU_MYSUFS_SUS_KSTAT
void mysufs_add_sus_kstat(void __user **user_info);
void mysufs_update_sus_kstat(void __user **user_info);
#endif

/* spoof_uname */
#ifdef CONFIG_MYSU_MYSUFS_SPOOF_UNAME
void mysufs_set_uname(void __user **user_info);
void mysufs_spoof_uname(struct new_utsname* tmp);
#endif

/* enable_log */
#ifdef CONFIG_MYSU_MYSUFS_ENABLE_LOG
void mysufs_enable_log(void __user **user_info);
#endif

/* spoof_cmdline_or_bootconfig */
#ifdef CONFIG_MYSU_MYSUFS_SPOOF_CMDLINE_OR_BOOTCONFIG
void mysufs_set_cmdline_or_bootconfig(void __user **user_info);
#endif

/* open_redirect */
#ifdef CONFIG_MYSU_MYSUFS_OPEN_REDIRECT
void mysufs_add_open_redirect(void __user **user_info);
#endif

/* sus_map */
#ifdef CONFIG_MYSU_MYSUFS_SUS_MAP
void mysufs_add_sus_map(void __user **user_info);
#endif

void mysufs_set_avc_log_spoofing(void __user **user_info);

void mysufs_get_enabled_features(void __user **user_info);
void mysufs_show_variant(void __user **user_info);
void mysufs_show_version(void __user **user_info);

void mysufs_start_sdcard_monitor_fn(void);

/* mysufs_init */
void mysufs_init(void);

#endif
