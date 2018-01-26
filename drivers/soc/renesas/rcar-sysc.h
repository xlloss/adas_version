/*
 * Renesas R-Car System Controller
 *
 * Copyright (C) 2016 Glider bvba
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */
#ifndef __SOC_RENESAS_RCAR_SYSC_H__
#define __SOC_RENESAS_RCAR_SYSC_H__

#include <linux/types.h>


/*
 * Power Domain flags
 */
#define PD_CPU		BIT(0)	/* Area contains main CPU core */
#define PD_SCU		BIT(1)	/* Area contains SCU and L2 cache */
#define PD_NO_CR	BIT(2)	/* Area lacks PWR{ON,OFF}CR registers */

#define PD_BUSY		BIT(3)	/* Busy, for internal use only */
#define PD_ON_ONCE	BIT(4)  /* Turned on once at boot */

#define PD_WA_CLK	BIT(5)  /* Use clocks for workaround */
#define PD_WA_CLK_RDY	BIT(6)  /* Clock ready, for internal use only */
#define PD_NO_INIT_ON	BIT(7)  /* Do not power on at initialization, for internal use */

#define PD_CPU_CR	PD_CPU		  /* CPU area has CR (R-Car H1) */
#define PD_CPU_NOCR	PD_CPU | PD_NO_CR /* CPU area lacks CR (R-Car Gen2/3) */
#define PD_ALWAYS_ON	PD_NO_CR	  /* Always-on area */

#define RCAR_SYSC_MAX_WA_CLKS	8

/*
 * Description of a Power Area
 */

struct rcar_sysc_area {
	const char *name;
	u16 chan_offs;		/* Offset of PWRSR register for this area */
	u8 chan_bit;		/* Bit in PWR* (except for PWRUP in PWRSR) */
	u8 isr_bit;		/* Bit in SYSCI*R */
	int parent;		/* -1 if none */
	unsigned int flags;	/* See PD_* */
	const char* wa_clk_names[RCAR_SYSC_MAX_WA_CLKS];	/* Clocks needed by workaround for A3 PD issue */
};


/*
 * SoC-specific Power Area Description
 */

struct rcar_sysc_info {
	const struct rcar_sysc_area *areas;
	unsigned int num_areas;
};

extern const struct rcar_sysc_info r8a7743_sysc_info;
extern const struct rcar_sysc_info r8a7745_sysc_info;
extern const struct rcar_sysc_info r8a7779_sysc_info;
extern const struct rcar_sysc_info r8a7790_sysc_info;
extern const struct rcar_sysc_info r8a7791_sysc_info;
extern const struct rcar_sysc_info r8a7792_sysc_info;
extern const struct rcar_sysc_info r8a7794_sysc_info;
extern const struct rcar_sysc_info r8a7795_sysc_info;
extern const struct rcar_sysc_info r8a7796_sysc_info;
extern const struct rcar_sysc_info r8a77965_sysc_info;
extern const struct rcar_sysc_info r8a7797_sysc_info;
extern const struct rcar_sysc_info r8a7798_sysc_info;
#endif /* __SOC_RENESAS_RCAR_SYSC_H__ */
