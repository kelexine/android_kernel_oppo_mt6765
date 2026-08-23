// SPDX-License-Identifier: GPL-2.0
/*
 * Script: ddp_dsi_diag.c
 * Author: kelexine <https://github.com/kelexine>
 * Target: Cubot P50 (MT6765, Kernel 4.19.127)
 * Purpose: Unconditional live DSI and panel diagnostic debugfs/procfs node.
 *          Bypasses _is_power_on_status() and is_mipi_enterulps() gates
 *          for bring-up debugging on Cubot P50 (ili9881h / MT6762R).
 */

#define LOG_TAG "DSI_DIAG"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif

#include "disp_drv_ddp.h"
#include "ddp_reg.h"
#include "ddp_hal.h"
#include "ddp_info.h"
#include "ddp_dsi.h"
#include "ddp_reg_dsi.h"
#include "disp_lcm.h"
#include "disp_lowpower.h"
#include "ddp_dsi_diag.h"

/* declared in primary_display.c */
struct disp_lcm_handle *primary_get_lcm(void);

#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *dsi_diag_debugfs_entry;
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
static struct proc_dir_entry *dsi_diag_procfs_entry;
#endif

static const char *_diag_dsi_mode_name(enum LCM_DSI_MODE_CON mode)
{
	switch (mode) {
	case CMD_MODE:
		return "CMD_MODE";
	case SYNC_PULSE_VDO_MODE:
		return "SYNC_PULSE_VDO_MODE";
	case SYNC_EVENT_VDO_MODE:
		return "SYNC_EVENT_VDO_MODE";
	case BURST_VDO_MODE:
		return "BURST_VDO_MODE";
	default:
		return "UNKNOWN";
	}
}

static const char *_diag_cmd_mode_state(unsigned int state)
{
	switch (state) {
	case 0x0001:
		return "idle";
	case 0x0002:
		return "Reading command queue for header";
	case 0x0004:
		return "Sending type-0 command";
	case 0x0008:
		return "Waiting frame data from RDMA for type-1 command";
	case 0x0010:
		return "Sending type-1 command";
	case 0x0020:
		return "Sending type-2 command";
	case 0x0040:
		return "Reading command queue for type-2 data";
	case 0x0080:
		return "Sending type-3 command";
	case 0x0100:
		return "Sending BTA";
	case 0x0200:
		return "Waiting RX-read data";
	case 0x0400:
		return "Waiting SW RACK for RX-read data";
	case 0x0800:
		return "Waiting TE";
	case 0x1000:
		return "Get TE";
	case 0x2000:
		return "Waiting SW RACK for TE";
	case 0x4000:
		return "Waiting external TE";
	case 0x8000:
		return "Get external TE";
	default:
		return "unknown/transient";
	}
}

static const char *_diag_vdo_mode_state(unsigned int state)
{
	switch (state) {
	case 0x0001:
		return "Video mode idle";
	case 0x0002:
		return "Sync start packet";
	case 0x0004:
		return "Hsync active";
	case 0x0008:
		return "Sync end packet";
	case 0x0010:
		return "Hsync back porch";
	case 0x0020:
		return "Video data period";
	case 0x0040:
		return "Hsync front porch";
	case 0x0080:
		return "BLLP";
	case 0x0200:
		return "Mix mode using command mode transmission";
	case 0x0400:
		return "Command transmission in BLLP";
	default:
		return "unknown/transient";
	}
}

static int dsi_diag_show(struct seq_file *s, void *v)
{
	struct timespec64 ts;
	struct disp_lcm_handle *plcm = NULL;
	struct LCM_DRIVER *lcm_drv = NULL;
	struct LCM_PARAMS *lcm_param = NULL;
	unsigned int pwr_status = 0;
	unsigned int ulps_status = 0;
	unsigned long dsi_base_va = 0;
	unsigned long dsi_base_pa = 0;
	unsigned long mipi_base_va = 0;
	unsigned long mipi_base_pa = 0;
	unsigned int dsi_clk = 0;
	unsigned int offset = 0;

	/* 1. Monotonic Timestamp */
	ktime_get_ts64(&ts);
	seq_printf(s, "==================================================\n");
	seq_printf(s, " DSI / PANEL LIVE DIAGNOSTIC DUMP (Cubot P50)\n");
	seq_printf(s, "==================================================\n");
	seq_printf(s, "Timestamp:            [%5llu.%06lu]\n",
		   (u64)ts.tv_sec, (unsigned long)(ts.tv_nsec / 1000));

	/* 2. Unconditional Raw Power & ULPS Status */
	pwr_status = _is_power_on_status(DISP_MODULE_DSI0);
	ulps_status = is_mipi_enterulps();

	seq_printf(s, "\n[POWER & ULPS STATUS]\n");
	seq_printf(s, "  _is_power_on_status(DSI0): %u (%s)\n",
		   pwr_status, pwr_status ? "POWER_ON" : "POWER_OFF");
	seq_printf(s, "  is_mipi_enterulps():       %u (%s)\n",
		   ulps_status, ulps_status ? "IN_ULPS" : "NORMAL");

	/* 3. LCM Driver State & Parameters */
	plcm = primary_get_lcm();
	seq_printf(s, "\n[LCM DRIVER STATUS]\n");
	if (plcm) {
		lcm_drv = plcm->drv;
		lcm_param = plcm->params;

		seq_printf(s, "  Handle:                    0x%pK\n", plcm);
		seq_printf(s, "  Driver Name:               %s\n",
			   (lcm_drv && lcm_drv->name) ? lcm_drv->name : mtkfb_lcm_name);
		seq_printf(s, "  disp_lcm_is_inited():      %d\n",
			   disp_lcm_is_inited(plcm));
		seq_printf(s, "  plcm->is_inited:           %d\n",
			   plcm->is_inited);
		seq_printf(s, "  Interface ID:              %d\n",
			   plcm->lcm_if_id);
		seq_printf(s, "  Index:                     %d\n",
			   plcm->index);

		if (lcm_param) {
			seq_printf(s, "  Resolution:                %u x %u (native: %u x %u)\n",
				   lcm_param->width, lcm_param->height,
				   plcm->lcm_original_width, plcm->lcm_original_height);
			seq_printf(s, "  Physical Size (mm):        %u x %u\n",
				   lcm_param->physical_width, lcm_param->physical_height);
			seq_printf(s, "  Type:                      %d (%s)\n",
				   lcm_param->type,
				   (lcm_param->type == LCM_TYPE_DSI) ? "DSI" :
				   (lcm_param->type == LCM_TYPE_DPI) ? "DPI" :
				   (lcm_param->type == LCM_TYPE_DBI) ? "DBI" : "OTHER");

			if (lcm_param->type == LCM_TYPE_DSI) {
				seq_printf(s, "  DSI Mode:                  %s (%d)\n",
					   _diag_dsi_mode_name(lcm_param->dsi.mode),
					   lcm_param->dsi.mode);
				seq_printf(s, "  Lanes:                     %d\n",
					   lcm_param->dsi.LANE_NUM);
				seq_printf(s, "  PLL / DSI Clock:           %u / %u kHz\n",
					   lcm_param->dsi.PLL_CLOCK,
					   lcm_param->dsi.dsi_clock);
				seq_printf(s, "  V Timing (act,vbp,vfp,vsa): %u, %u, %u, %u\n",
					   lcm_param->dsi.vertical_active_line,
					   lcm_param->dsi.vertical_backporch,
					   lcm_param->dsi.vertical_frontporch,
					   lcm_param->dsi.vertical_sync_active);
				seq_printf(s, "  H Timing (act,hbp,hfp,hsa): %u, %u, %u, %u\n",
					   lcm_param->width,
					   lcm_param->dsi.horizontal_backporch,
					   lcm_param->dsi.horizontal_frontporch,
					   lcm_param->dsi.horizontal_sync_active);
			}
		}

		/* lcm_compare_id safety check */
		seq_printf(s, "  compare_id Callback:       %s\n",
			   (lcm_drv && lcm_drv->compare_id) ? "AVAILABLE" : "NULL");
		if (lcm_drv && lcm_drv->compare_id) {
			if (!pwr_status || ulps_status) {
				seq_printf(s, "  compare_id Safe Execution: NO (DSI power off or in ULPS)\n");
			} else if (plcm->is_inited) {
				seq_printf(s, "  compare_id Safe Execution: SKIPPED (LCM active; avoiding hardware RESET line toggle)\n");
			} else {
				seq_printf(s, "  compare_id Safe Execution: SAFE TO PROBE\n");
			}
		}
	} else {
		seq_printf(s, "  Primary LCM Handle:        NULL (fallback mtkfb_lcm_name: %s)\n",
			   mtkfb_lcm_name);
	}

	/* 4. Trigger kernel dmesg DSI analysis and register dump unconditionally */
	pr_info("[DSI_DIAG] Live diagnostic triggered from %s (pwr=%u, ulps=%u)\n",
		file_dentry(s->file)->d_name.name, pwr_status, ulps_status);
	dsi_analysis(DISP_MODULE_DSI0);
	DSI_DumpRegisters(DISP_MODULE_DSI0, 1);

	/* 5. Format DSI Hardware Register State directly into seq_file */
	dsi_base_va = ddp_get_module_va(DISP_MODULE_DSI0);
	dsi_base_pa = ddp_get_module_pa(DISP_MODULE_DSI0);
	mipi_base_va = ddp_get_module_va(DISP_MODULE_MIPI0);
	mipi_base_pa = ddp_get_module_pa(DISP_MODULE_MIPI0);
	dsi_clk = dsi_phy_get_clk(DISP_MODULE_DSI0);

	seq_printf(s, "\n[DSI HARDWARE REGISTERS & ENGINE STATUS]\n");
	seq_printf(s, "  DSI0 Base Address:         VA=0x%lx, PA=0x%lx\n",
		   dsi_base_va, dsi_base_pa);
	seq_printf(s, "  MIPI0 Base Address:        VA=0x%lx, PA=0x%lx\n",
		   mipi_base_va, mipi_base_pa);
	seq_printf(s, "  MIPITX PHY Clock:          %u kHz\n", dsi_clk);

	if (dsi_base_va) {
		u32 dsi_start = INREG32(dsi_base_va + 0x0000);
		u32 dsi_sta = INREG32(dsi_base_va + 0x0004);
		u32 dsi_inten = INREG32(dsi_base_va + 0x0008);
		u32 dsi_intsta = INREG32(dsi_base_va + 0x000C);
		u32 dsi_com_ctrl = INREG32(dsi_base_va + 0x0010);
		u32 dsi_mode_ctrl = INREG32(dsi_base_va + 0x0014);
		u32 dsi_txrx_ctrl = INREG32(dsi_base_va + 0x0018);
		u32 dsi_lccon = INREG32(dsi_base_va + 0x0104);
		u32 dsi_dbg6 = INREG32(dsi_base_va + 0x0160) & 0xFFFF;
		u32 dsi_dbg7 = INREG32(dsi_base_va + 0x0164) & 0xFF;
		u32 dsi_dbg8 = INREG32(dsi_base_va + 0x0168) & 0x3FFF;
		u32 dsi_dbg9 = INREG32(dsi_base_va + 0x016C) & 0x3FFFFF;

		seq_printf(s, "  DSI_START:                 0x%08x (START=%u, VM_CMD_START=%u)\n",
			   dsi_start, dsi_start & 0x1, (dsi_start >> 1) & 0x1);
		seq_printf(s, "  DSI_STA:                   0x%08x\n", dsi_sta);
		seq_printf(s, "  DSI_INTEN:                 0x%08x\n", dsi_inten);
		seq_printf(s, "  DSI_INTSTA:                0x%08x (BUSY=%u, RD_RDY=%u, CMD_DONE=%u, TE_RDY=%u, VM_DONE=%u)\n",
			   dsi_intsta,
			   (dsi_intsta >> 31) & 0x1,
			   (dsi_intsta >> 0) & 0x1,
			   (dsi_intsta >> 1) & 0x1,
			   (dsi_intsta >> 2) & 0x1,
			   (dsi_intsta >> 4) & 0x1);
		seq_printf(s, "  DSI_COM_CTRL:              0x%08x (DSI_EN=%u, DSI_DUAL_EN=%u)\n",
			   dsi_com_ctrl, (dsi_com_ctrl >> 0) & 0x1, (dsi_com_ctrl >> 1) & 0x1);
		seq_printf(s, "  DSI_MODE_CTRL:             0x%08x (MODE=%s)\n",
			   dsi_mode_ctrl, _diag_dsi_mode_name(dsi_mode_ctrl & 0x3));
		seq_printf(s, "  DSI_TXRX_CTRL:             0x%08x (LANE_NUM=%u, EXT_TE_EN=%u, CKLP_EN=%u)\n",
			   dsi_txrx_ctrl,
			   (dsi_txrx_ctrl >> 2) & 0x3,
			   (dsi_txrx_ctrl >> 24) & 0x1,
			   (dsi_txrx_ctrl >> 6) & 0x1);
		seq_printf(s, "  DSI_PHY_LCCON:             0x%08x (LC_HS_TX_EN=%u)\n",
			   dsi_lccon, (dsi_lccon >> 0) & 0x1);
		seq_printf(s, "  DSI State (CMD Mode dbg6): 0x%04x (%s)\n",
			   dsi_dbg6, _diag_cmd_mode_state(dsi_dbg6));
		seq_printf(s, "  DSI State (VDO Mode dbg7): 0x%02x (%s)\n",
			   dsi_dbg7, _diag_vdo_mode_state(dsi_dbg7));
		seq_printf(s, "  DSI Word Counter (dbg8):   0x%04x (%s)\n",
			   dsi_dbg8, _diag_cmd_mode_state(dsi_dbg8));
		seq_printf(s, "  DSI Line Counter (dbg9):   0x%06x (%s)\n",
			   dsi_dbg9, _diag_cmd_mode_state(dsi_dbg9));

		/* 6. Raw DSI Register Map Dump */
		seq_printf(s, "\n[DSI0 RAW REGISTER DUMP (0x0000 - 0x0178)]\n");
		for (offset = 0; offset < sizeof(struct DSI_REGS); offset += 16) {
			seq_printf(s, "  0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				   offset,
				   INREG32(dsi_base_va + offset),
				   INREG32(dsi_base_va + offset + 0x4),
				   INREG32(dsi_base_va + offset + 0x8),
				   INREG32(dsi_base_va + offset + 0xC));
		}

		/* 7. DSI Command Queue Registers */
		seq_printf(s, "\n[DSI0 CMD QUEUE REGISTERS (0x0200 - 0x0220)]\n");
		for (offset = 0; offset < 32; offset += 16) {
			seq_printf(s, "  0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				   0x200 + offset,
				   INREG32(dsi_base_va + 0x200 + offset),
				   INREG32(dsi_base_va + 0x200 + offset + 0x4),
				   INREG32(dsi_base_va + 0x200 + offset + 0x8),
				   INREG32(dsi_base_va + 0x200 + offset + 0xC));
		}
	}

	if (mipi_base_va) {
		seq_printf(s, "\n[DSI_PHY0 / MIPITX0 RAW REGISTER DUMP (0x0000 - 0x06A0)]\n");
		for (offset = 0; offset < 0x6A0; offset += 16) {
			seq_printf(s, "  0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				   offset,
				   INREG32(mipi_base_va + offset),
				   INREG32(mipi_base_va + offset + 0x4),
				   INREG32(mipi_base_va + offset + 0x8),
				   INREG32(mipi_base_va + offset + 0xC));
		}
	}

	seq_printf(s, "==================================================\n");
	seq_printf(s, " END OF DSI DIAGNOSTIC DUMP\n");
	seq_printf(s, "==================================================\n");

	return 0;
}

static int dsi_diag_open(struct inode *inode, struct file *file)
{
	return single_open(file, dsi_diag_show, inode->i_private);
}

static const struct file_operations dsi_diag_fops = {
	.owner = THIS_MODULE,
	.open = dsi_diag_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void ddp_dsi_diag_init(struct dentry *parent_debugfs,
		       struct proc_dir_entry *parent_procfs)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	if (parent_debugfs) {
		dsi_diag_debugfs_entry = debugfs_create_file("dsi_diag",
							     S_IFREG | 0444,
							     parent_debugfs,
							     NULL,
							     &dsi_diag_fops);
		if (!dsi_diag_debugfs_entry)
			pr_err("[DSI_DIAG] failed to create /d/disp/dsi_diag\n");
		else
			pr_info("[DSI_DIAG] registered /d/disp/dsi_diag\n");
	}
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	if (parent_procfs) {
		dsi_diag_procfs_entry = proc_create("dsi_diag",
						    S_IFREG | 0444,
						    parent_procfs,
						    &dsi_diag_fops);
		if (!dsi_diag_procfs_entry)
			pr_err("[DSI_DIAG] failed to create /proc/disp/dsi_diag\n");
		else
			pr_info("[DSI_DIAG] registered /proc/disp/dsi_diag\n");
	}
#endif
}

void ddp_dsi_diag_exit(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	if (dsi_diag_debugfs_entry) {
		debugfs_remove(dsi_diag_debugfs_entry);
		dsi_diag_debugfs_entry = NULL;
	}
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	if (dsi_diag_procfs_entry) {
		proc_remove(dsi_diag_procfs_entry);
		dsi_diag_procfs_entry = NULL;
	}
#endif
}
