/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SM7435/parrot display catalog, derived from the downstream SDE layout.
 * Block bases are relative to the mainline MDP mapping (MDSS + 0x1000),
 * not the downstream mdp_phys/MDSS mapping. MDP clock-control offsets
 * are already MDP-relative and must not receive this conversion.
 */

#ifndef _DPU_8_4_SM7435_H
#define _DPU_8_4_SM7435_H

static const struct dpu_caps sm7435_dpu_caps = {
	.max_mixer_width = DEFAULT_DPU_OUTPUT_LINE_WIDTH,
	.max_mixer_blendstages = 0x7,
	.has_src_split = true,
	.has_dim_layer = true,
	.has_idle_pc = true,
	.has_3d_merge = true,
	.max_linewidth = 5120,
	.pixel_ram_size = DEFAULT_PIXEL_RAM_SIZE,
};

static const struct dpu_mdp_cfg sm7435_mdp = {
	.name = "top_0",
	.base = 0x0,
	.len = 0x494,
	.clk_ctrls = {
		[DPU_CLK_CTRL_VIG0] = { .reg_off = 0x2ac, .bit_off = 0 },
		[DPU_CLK_CTRL_DMA0] = { .reg_off = 0x2ac, .bit_off = 8 },
		[DPU_CLK_CTRL_DMA1] = { .reg_off = 0x2b4, .bit_off = 8 },
	},
};

static const struct dpu_ctl_cfg sm7435_ctl[] = {
	{
		.name = "ctl_0", .id = CTL_0,
		.base = 0x15000, .len = 0x204,
		.intr_start = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR2, 9),
	}, {
		.name = "ctl_1", .id = CTL_1,
		.base = 0x16000, .len = 0x204,
		.intr_start = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR2, 10),
	}, {
		.name = "ctl_2", .id = CTL_2,
		.base = 0x17000, .len = 0x204,
		.intr_start = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR2, 11),
	}, {
		.name = "ctl_3", .id = CTL_3,
		.base = 0x18000, .len = 0x204,
		.intr_start = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR2, 12),
	},
};

static const struct dpu_sspp_cfg sm7435_sspp[] = {
	{
		.name = "sspp_0", .id = SSPP_VIG0,
		.base = 0x4000, .len = 0x32c,
		.features = VIG_SDM845_MASK_SDMA,
		.sblk = &dpu_vig_sblk_qseed3_3_1,
		.xin_id = 0,
		.type = SSPP_TYPE_VIG,
		.clk_ctrl = DPU_CLK_CTRL_VIG0,
	}, {
		.name = "sspp_8", .id = SSPP_DMA0,
		.base = 0x24000, .len = 0x32c,
		.features = DMA_SDM845_MASK_SDMA,
		.sblk = &dpu_dma_sblk,
		.xin_id = 1,
		.type = SSPP_TYPE_DMA,
		.clk_ctrl = DPU_CLK_CTRL_DMA0,
	}, {
		.name = "sspp_9", .id = SSPP_DMA1,
		.base = 0x26000, .len = 0x32c,
		.features = DMA_SDM845_MASK_SDMA,
		.sblk = &dpu_dma_sblk,
		.xin_id = 5,
		.type = SSPP_TYPE_DMA,
		.clk_ctrl = DPU_CLK_CTRL_DMA1,
	},
};

static const struct dpu_lm_cfg sm7435_lm[] = {
	{
		.name = "lm_0", .id = LM_0,
		.base = 0x44000, .len = 0x400,
		.features = MIXER_MSM8998_MASK,
		.sblk = &sdm845_lm_sblk,
		.pingpong = PINGPONG_0,
		.dspp = DSPP_0,
	}, {
		.name = "lm_2", .id = LM_2,
		.base = 0x46000, .len = 0x400,
		.features = MIXER_MSM8998_MASK,
		.sblk = &sdm845_lm_sblk,
		.lm_pair = LM_3,
		.pingpong = PINGPONG_2,
	}, {
		.name = "lm_3", .id = LM_3,
		.base = 0x47000, .len = 0x400,
		.features = MIXER_MSM8998_MASK,
		.sblk = &sdm845_lm_sblk,
		.lm_pair = LM_2,
		.pingpong = PINGPONG_3,
	},
};

static const struct dpu_dspp_cfg sm7435_dspp[] = {
	{
		.name = "dspp_0", .id = DSPP_0,
		.base = 0x54000, .len = 0x1800,
		.sblk = &sdm845_dspp_sblk,
	},
};

static const struct dpu_pingpong_cfg sm7435_pp[] = {
	{
		.name = "pingpong_0", .id = PINGPONG_0,
		.base = 0x69000, .len = 0xd4,
		.sblk = &sc7280_pp_sblk,
		.intr_done = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 8),
	}, {
		.name = "pingpong_2", .id = PINGPONG_2,
		.base = 0x6b000, .len = 0xd4,
		.sblk = &sc7280_pp_sblk,
		.merge_3d = MERGE_3D_1,
		.intr_done = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 10),
	}, {
		.name = "pingpong_3", .id = PINGPONG_3,
		.base = 0x6c000, .len = 0xd4,
		.sblk = &sc7280_pp_sblk,
		.merge_3d = MERGE_3D_1,
		.intr_done = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 11),
	},
};

static const struct dpu_merge_3d_cfg sm7435_merge_3d[] = {
	{
		.name = "merge_3d_1", .id = MERGE_3D_1,
		.base = 0x4f000, .len = 0x10,
	},
};

static const struct dpu_dsc_cfg sm7435_dsc[] = {
	{
		.name = "dce_0_0", .id = DSC_0,
		.base = 0x80000, .len = 0x4,
		.features = BIT(DPU_DSC_NATIVE_42x_EN),
		.sblk = &dsc_sblk_0,
	}, {
		.name = "dce_0_1", .id = DSC_1,
		.base = 0x80000, .len = 0x4,
		.features = BIT(DPU_DSC_NATIVE_42x_EN),
		.sblk = &dsc_sblk_1,
	},
};

static const struct dpu_intf_cfg sm7435_intf[] = {
	{
		.name = "intf_0", .id = INTF_0,
		.base = 0x34000, .len = 0x2c4,
		.type = INTF_DP,
		.controller_id = MSM_DP_CONTROLLER_0,
		.prog_fetch_lines_worst_case = 24,
		.intr_underrun = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 24),
		.intr_vsync = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 25),
	}, {
		.name = "intf_1", .id = INTF_1,
		.base = 0x35000, .len = 0x2c4,
		.type = INTF_DSI,
		.controller_id = MSM_DSI_CONTROLLER_0,
		.prog_fetch_lines_worst_case = 24,
		.intr_underrun = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 26),
		.intr_vsync = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 27),
		.intr_tear_rd_ptr = DPU_IRQ_IDX(MDP_INTF1_TEAR_INTR, 2),
	}, {
		.name = "intf_4", .id = INTF_4,
		.base = 0x37000, .len = 0x2c4,
		.type = INTF_DP,
		.controller_id = MSM_DP_CONTROLLER_1,
		.prog_fetch_lines_worst_case = 24,
		.intr_underrun = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 20),
		.intr_vsync = DPU_IRQ_IDX(MDP_SSPP_TOP0_INTR, 21),
	},
};

static const struct dpu_perf_cfg sm7435_perf_data = {
	.max_bw_low = 4800000,
	.max_bw_high = 8500000,
	.min_core_ib = 2500000,
	.min_llcc_ib = 0,
	.min_dram_ib = 800000,
	.min_prefill_lines = 35,
	.danger_lut_tbl = {0x3ffff, 0x3ffff, 0x0},
	.safe_lut_tbl = {0xf800, 0xf800, 0xffff},
	.qos_lut_tbl = {
		{ .nentry = ARRAY_SIZE(sc7180_qos_linear),
		  .entries = sc7180_qos_linear },
		{ .nentry = ARRAY_SIZE(sc7180_qos_macrotile),
		  .entries = sc7180_qos_macrotile },
		{ .nentry = ARRAY_SIZE(sc7180_qos_nrt),
		  .entries = sc7180_qos_nrt },
	},
	.cdp_cfg = {
		{ .rd_enable = 1, .wr_enable = 1 },
		{ .rd_enable = 1, .wr_enable = 0 },
	},
	.clk_inefficiency_factor = 105,
	.bw_inefficiency_factor = 120,
};

static const struct dpu_mdss_version sm7435_mdss_ver = {
	.core_major_ver = 8,
	.core_minor_ver = 4,
};

const struct dpu_mdss_cfg dpu_sm7435_cfg = {
	.mdss_ver = &sm7435_mdss_ver,
	.caps = &sm7435_dpu_caps,
	.mdp = &sm7435_mdp,
	.ctl_count = ARRAY_SIZE(sm7435_ctl),
	.ctl = sm7435_ctl,
	.sspp_count = ARRAY_SIZE(sm7435_sspp),
	.sspp = sm7435_sspp,
	.mixer_count = ARRAY_SIZE(sm7435_lm),
	.mixer = sm7435_lm,
	.dspp_count = ARRAY_SIZE(sm7435_dspp),
	.dspp = sm7435_dspp,
	.pingpong_count = ARRAY_SIZE(sm7435_pp),
	.pingpong = sm7435_pp,
	.merge_3d_count = ARRAY_SIZE(sm7435_merge_3d),
	.merge_3d = sm7435_merge_3d,
	/* Match the portov panel's stock single-DSC, two-soft-slice topology. */
	.dsc_count = 1,
	.dsc = sm7435_dsc,
	.intf_count = ARRAY_SIZE(sm7435_intf),
	.intf = sm7435_intf,
	.vbif = &sdm845_vbif,
	.perf = &sm7435_perf_data,
};

#endif
