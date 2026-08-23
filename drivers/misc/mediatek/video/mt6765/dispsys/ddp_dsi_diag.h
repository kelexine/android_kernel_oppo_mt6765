/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Script: ddp_dsi_diag.h
 * Author: kelexine <https://github.com/kelexine>
 * Target: Cubot P50 (MT6765, Kernel 4.19.127)
 * Purpose: Header for DSI/panel live diagnostics debugfs/procfs node.
 */

#ifndef __DDP_DSI_DIAG_H__
#define __DDP_DSI_DIAG_H__

#include <linux/types.h>

struct dentry;
struct proc_dir_entry;

#if IS_ENABLED(CONFIG_MTK_DSI_DIAG)
void ddp_dsi_diag_init(struct dentry *parent_debugfs,
		       struct proc_dir_entry *parent_procfs);
void ddp_dsi_diag_exit(void);
#else
static inline void ddp_dsi_diag_init(struct dentry *parent_debugfs,
				     struct proc_dir_entry *parent_procfs) {}
static inline void ddp_dsi_diag_exit(void) {}
#endif

#endif /* __DDP_DSI_DIAG_H__ */
