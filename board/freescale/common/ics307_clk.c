// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 */

#include <config.h>
#include <clock_legacy.h>
#include <log.h>
#include <asm/io.h>

#include "ics307_clk.h"

#if defined(CONFIG_FSL_NGPIXIS)
#include "ngpixis.h"
#define fpga_reg pixis
#elif defined(CONFIG_FSL_QIXIS)
#include "qixis.h"
#define fpga_reg ((struct qixis *)QIXIS_BASE)
#else
#include "pixis.h"
#define fpga_reg pixis
#endif

/* define for SYS CLK or CLK1Frequency */
#define TTL		1
#define CLK2		0
#define CRYSTAL		0
#define MAX_VDW		(511 + 8)
#define MAX_RDW		(127 + 2)
#define MIN_VDW		(4 + 8)
#define MIN_RDW		(1 + 2)
#define NUM_OD_SETTING	8
/*
 * These defines cover the industrial temperature range part,
 * for commercial, change below to 400000 and 55000, respectively
 */
#define MAX_VCO		360000
#define MIN_VCO		60000

/* decode S[0-2] to Output Divider (OD) */
static u8 ics307_s_to_od[] = {
	10, 2, 8, 4, 5, 7, 3, 6
};

/*
 * Find one solution to generate required frequency for SYSCLK
 * out_freq: KHz, required frequency to the SYSCLK
 * the result will be retuned with component RDW, VDW, OD, TTL,
 * CLK2 and crystal
 */
unsigned long ics307_sysclk_calculator(unsigned long out_freq)
{
	const unsigned long input_freq = CFG_ICS307_REFCLK_HZ;
	unsigned long vdw, rdw, odp, s_vdw = 0, s_rdw = 0, s_odp = 0, od;
	unsigned long tmp_out, diff, result = 0;
	int found = 0;

	for (odp = 0; odp < NUM_OD_SETTING; odp++) {
		od = ics307_s_to_od[odp];
		if (od * out_freq < MIN_VCO || od * out_freq > MAX_VCO)
			continue;
		for (rdw = MIN_RDW; rdw <= MAX_RDW; rdw++) {
			/* Calculate the VDW */
			vdw = out_freq * 1000 * od * rdw / (input_freq * 2);
			if (vdw > MAX_VDW)
				vdw = MAX_VDW;
			if (vdw < MIN_VDW)
				continue;
			/* Calculate the temp out frequency */
			tmp_out = input_freq * 2 * vdw / (rdw * od * 1000);
			diff = max(out_freq, tmp_out) - min(out_freq, tmp_out);
			/*
			 * calculate the percent, the precision is 1/1000
			 * If greater than 1/1000, continue
			 * otherwise, we think the solution is we required
			 */
			if (diff * 1000 / out_freq > 1)
				continue;
			else {
				s_vdw = vdw;
				s_rdw = rdw;
				s_odp = odp;
				found = 1;
				break;
			}
		}
	}

	if (found)
		result = (s_rdw - 2) | (s_vdw - 8) << 7 | s_odp << 16 |
			CLK2 << 19 | TTL << 21 | CRYSTAL << 22;

	debug("ICS307-02: RDW: %ld, VDW: %ld, OD: %d\n", s_rdw - 2, s_vdw - 8,
			ics307_s_to_od[s_odp]);
	return result;
}

/*
 * Calculate frequency being generated by ICS307-02 clock chip based upon
 * the control bytes being programmed into it.
 */
static unsigned long ics307_clk_freq(u8 cw0, u8 cw1, u8 cw2)
{
	const unsigned long input_freq = CFG_ICS307_REFCLK_HZ;
	unsigned long vdw = ((cw1 << 1) & 0x1FE) + ((cw2 >> 7) & 1);
	unsigned long rdw = cw2 & 0x7F;
	unsigned long od = ics307_s_to_od[cw0 & 0x7];
	unsigned long freq;

	/*
	 * CLK1 Freq = Input Frequency * 2 * (VDW + 8) / ((RDW + 2) * OD)
	 *
	 * cw0:  C1 C0 TTL F1 F0 S2 S1 S0
	 * cw1:  V8 V7 V6 V5 V4 V3 V2 V1
	 * cw2:  V0 R6 R5 R4 R3 R2 R1 R0
	 *
	 * R6:R0 = Reference Divider Word (RDW)
	 * V8:V0 = VCO Divider Word (VDW)
	 * S2:S0 = Output Divider Select (OD)
	 * F1:F0 = Function of CLK2 Output
	 * TTL = duty cycle
	 * C1:C0 = internal load capacitance for cyrstal
	 *
	 */

	freq = input_freq * 2 * (vdw + 8) / ((rdw + 2) * od);

	debug("ICS307: CW[0-2]: %02X %02X %02X => %lu Hz\n", cw0, cw1, cw2,
			freq);
	return freq;
}

unsigned long get_board_sys_clk(void)
{
	return ics307_clk_freq(
			in_8(&fpga_reg->sclk[0]),
			in_8(&fpga_reg->sclk[1]),
			in_8(&fpga_reg->sclk[2]));
}

#ifdef CONFIG_DYNAMIC_DDR_CLK_FREQ
unsigned long get_board_ddr_clk(void)
{
	return ics307_clk_freq(
			in_8(&fpga_reg->dclk[0]),
			in_8(&fpga_reg->dclk[1]),
			in_8(&fpga_reg->dclk[2]));
}
#endif
