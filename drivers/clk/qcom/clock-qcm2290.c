// SPDX-License-Identifier: BSD-3-Clause
/*
 * Clock drivers for Qualcomm qcm2290
 *
 * (C) Copyright 2024 Linaro Ltd.
 */

#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <dt-bindings/clock/qcom,gcc-qcm2290.h>

#include "clock-qcom.h"

#define QUPV3_WRAP0_S4_CMD_RCGR 0x1f608
#define SDCC2_APPS_CLK_CMD_RCGR 0x1e00c

static const struct freq_tbl ftbl_gcc_qupv3_wrap0_s0_clk_src[] = {
	F(7372800, CFG_CLK_SRC_GPLL0_AUX2, 1, 384, 15625),
	F(14745600, CFG_CLK_SRC_GPLL0_AUX2, 1, 768, 15625),
	F(19200000, CFG_CLK_SRC_CXO, 1, 0, 0),
	F(29491200, CFG_CLK_SRC_GPLL0_AUX2, 1, 1536, 15625),
	F(32000000, CFG_CLK_SRC_GPLL0_AUX2, 1, 8, 75),
	F(48000000, CFG_CLK_SRC_GPLL0_AUX2, 1, 4, 25),
	F(64000000, CFG_CLK_SRC_GPLL0_AUX2, 1, 16, 75),
	F(75000000, CFG_CLK_SRC_GPLL0_AUX2, 4, 0, 0),
	F(80000000, CFG_CLK_SRC_GPLL0_AUX2, 1, 4, 15),
	F(96000000, CFG_CLK_SRC_GPLL0_AUX2, 1, 8, 25),
	F(100000000, CFG_CLK_SRC_GPLL0_AUX2, 3, 0, 0),
	F(102400000, CFG_CLK_SRC_GPLL0_AUX2, 1, 128, 375),
	F(112000000, CFG_CLK_SRC_GPLL0_AUX2, 1, 28, 75),
	F(117964800, CFG_CLK_SRC_GPLL0_AUX2, 1, 6144, 15625),
	F(120000000, CFG_CLK_SRC_GPLL0_AUX2, 2.5, 0, 0),
	F(128000000, CFG_CLK_SRC_GPLL6, 3, 0, 0),
	{}
};

static const struct freq_tbl ftbl_gcc_sdcc2_apps_clk_src[] = {
	F(400000, CFG_CLK_SRC_CXO, 12, 1, 4),
	F(19200000, CFG_CLK_SRC_CXO, 1, 0, 0),
	F(25000000, CFG_CLK_SRC_GPLL0_AUX2, 12, 0, 0),
	F(50000000, CFG_CLK_SRC_GPLL0_AUX2, 6, 0, 0),
	F(100000000, CFG_CLK_SRC_GPLL0_AUX2, 3, 0, 0),
	F(202000000, CFG_CLK_SRC_GPLL7, 4, 0, 0), // 6.5, 1, 4
	{}
};

static const struct pll_vote_clk gpll7_clk = {
	.status = 0x7000,
	.status_bit = BIT(31),
	.ena_vote = 0x79000,
	.vote_bit = BIT(7),
};

static const struct gate_clk qcm2290_clks[] = {
	GATE_CLK(GCC_AHB2PHY_USB_CLK, 0x1d008, 0x00000001),
	GATE_CLK(GCC_CFG_NOC_USB3_PRIM_AXI_CLK, 0x1a084, 0x00000001),
	GATE_CLK(GCC_QUPV3_WRAP0_CORE_2X_CLK, 0x7900c, 0x00000200),
	GATE_CLK(GCC_QUPV3_WRAP0_CORE_CLK, 0x7900c, 0x00000100),
	GATE_CLK(GCC_QUPV3_WRAP0_S0_CLK, 0x7900c, 0x00000400),
	GATE_CLK(GCC_QUPV3_WRAP0_S1_CLK, 0x7900c, 0x00000800),
	GATE_CLK(GCC_QUPV3_WRAP0_S2_CLK, 0x7900c, 0x00001000),
	GATE_CLK(GCC_QUPV3_WRAP0_S3_CLK, 0x7900c, 0x00002000),
	GATE_CLK(GCC_QUPV3_WRAP0_S4_CLK, 0x7900c, 0x00004000),
	GATE_CLK(GCC_QUPV3_WRAP0_S5_CLK, 0x7900c, 0x00008000),
	GATE_CLK(GCC_QUPV3_WRAP_0_M_AHB_CLK, 0x7900c, 0x00000040),
	GATE_CLK(GCC_QUPV3_WRAP_0_S_AHB_CLK, 0x7900c, 0x00000080),
	GATE_CLK(GCC_SDCC1_AHB_CLK, 0x38008, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK, 0x38004, 0x00000001),
	GATE_CLK(GCC_SDCC1_ICE_CORE_CLK, 0x3800c, 0x00000001),
	GATE_CLK(GCC_SDCC2_AHB_CLK, 0x1e008, 0x00000001),
	GATE_CLK(GCC_SDCC2_APPS_CLK, 0x1e004, 0x00000001),
	GATE_CLK(GCC_SYS_NOC_CPUSS_AHB_CLK, 0x79004, 0x00000001),
	GATE_CLK(GCC_SYS_NOC_USB3_PRIM_AXI_CLK, 0x1a080, 0x00000001),
	GATE_CLK(GCC_USB30_PRIM_MASTER_CLK, 0x1a010, 0x00000001),
	GATE_CLK(GCC_USB30_PRIM_MOCK_UTMI_CLK, 0x1a018, 0x00000001),
	GATE_CLK(GCC_USB30_PRIM_SLEEP_CLK, 0x1a014, 0x00000001),
	GATE_CLK(GCC_USB3_PRIM_CLKREF_CLK, 0x9f000, 0x00000001),
	GATE_CLK(GCC_USB3_PRIM_PHY_COM_AUX_CLK, 0x1a054, 0x00000001),
	GATE_CLK(GCC_USB3_PRIM_PHY_PIPE_CLK, 0x1a058, 0x00000001),
};

static ulong qcm2290_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	const struct freq_tbl *freq;

	debug("%s: clk %s rate %lu\n", __func__, clk->dev->name, rate);

	switch (clk->id) {
	case GCC_QUPV3_WRAP0_S4_CLK: /*UART2*/
		freq = qcom_find_freq(ftbl_gcc_qupv3_wrap0_s0_clk_src, rate);
		clk_rcg_set_rate_mnd(priv->base, QUPV3_WRAP0_S4_CMD_RCGR,
				     freq->pre_div, freq->m, freq->n, freq->src,
				     16);
		return 0;
	case GCC_SDCC2_APPS_CLK:
		/* Enable GPLL7 so we can point SDCC2_APPS_CLK_SRC RCG at it */
		clk_enable_gpll0(priv->base, &gpll7_clk);
		freq = qcom_find_freq(ftbl_gcc_sdcc2_apps_clk_src, rate);
		WARN(freq->src != CFG_CLK_SRC_GPLL7,
		     "SDCC2_APPS_CLK_SRC not set to GPLL7, requested rate %lu\n",
		     rate);
		clk_rcg_set_rate_mnd(priv->base, SDCC2_APPS_CLK_CMD_RCGR,
				     freq->pre_div, freq->m, freq->n, freq->src,
				     8);
		return freq->freq;
	case GCC_SDCC1_APPS_CLK:
		/* The firmware turns this on for us and always sets it to this rate */
		return 384000000;
	default:
		return 0;
	}
}

static int qcm2290_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks < clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, qcm2290_clks[clk->id].name);

	switch (clk->id) {
	case GCC_USB30_PRIM_MASTER_CLK:
		qcom_gate_clk_en(priv, GCC_USB3_PRIM_PHY_COM_AUX_CLK);
		qcom_gate_clk_en(priv, GCC_USB3_PRIM_CLKREF_CLK);
		break;
	}

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map qcm2290_gcc_resets[] = {
	[GCC_CAMSS_OPE_BCR] = { 0x55000 },
	[GCC_CAMSS_TFE_BCR] = { 0x52000 },
	[GCC_CAMSS_TOP_BCR] = { 0x58000 },
	[GCC_GPU_BCR] = { 0x36000 },
	[GCC_MMSS_BCR] = { 0x17000 },
	[GCC_PDM_BCR] = { 0x20000 },
	[GCC_QUPV3_WRAPPER_0_BCR] = { 0x1f000 },
	[GCC_QUSB2PHY_PRIM_BCR] = { 0x1c000 },
	[GCC_SDCC1_BCR] = { 0x38000 },
	[GCC_SDCC2_BCR] = { 0x1e000 },
	[GCC_USB30_PRIM_BCR] = { 0x1a000 },
	[GCC_USB3_PHY_PRIM_SP0_BCR] = { 0x1b000 },
	[GCC_USB3PHY_PHY_PRIM_SP0_BCR] = { 0x1b008 },
	[GCC_USB_PHY_CFG_AHB2PHY_BCR] = { 0x1d000 },
	[GCC_VCODEC0_BCR] = { 0x58094 },
	[GCC_VENUS_BCR] = { 0x58078 },
	[GCC_VIDEO_INTERFACE_BCR] = { 0x6e000 },
};

static const struct qcom_power_map qcm2290_gdscs[] = {
	[GCC_USB30_PRIM_GDSC] = { 0x1a004 },
};

static struct msm_clk_data qcm2290_gcc_data = {
	.resets = qcm2290_gcc_resets,
	.num_resets = ARRAY_SIZE(qcm2290_gcc_resets),
	.clks = qcm2290_clks,
	.num_clks = ARRAY_SIZE(qcm2290_clks),
	.power_domains = qcm2290_gdscs,
	.num_power_domains = ARRAY_SIZE(qcm2290_gdscs),

	.enable = qcm2290_enable,
	.set_rate = qcm2290_set_rate,
};

static const struct udevice_id gcc_qcm2290_of_match[] = {
	{
		.compatible = "qcom,gcc-qcm2290",
		.data = (ulong)&qcm2290_gcc_data,
	},
	{}
};

U_BOOT_DRIVER(gcc_qcm2290) = {
	.name = "gcc_qcm2290",
	.id = UCLASS_NOP,
	.of_match = gcc_qcm2290_of_match,
	.bind = qcom_cc_bind,
	.flags = DM_FLAG_PRE_RELOC,
};
