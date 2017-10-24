/* drivers/cpufreq/sdp1404-big-cpufreq.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * SDP1404 Hawk-P big cluster - CPU frequency scaling support
 *
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/power/sdp_asv.h>
#include <linux/platform_data/sdp-cpufreq.h>

#include <asm/cacheflush.h>

#include <mach/map.h>
#include <mach/soc.h>

#define CPUFREQ_LEVEL_END	L15

#ifdef MAX_CPU_ASV_GROUP
#define CPUFREQ_ASV_COUNT	MAX_CPU_ASV_GROUP
#else
#define CPUFREQ_ASV_COUNT	10
#endif

extern bool sdp_cpufreq_print_on;

static int hawkp_revision_id;

static DEFINE_SPINLOCK(freq_lock);

static void __iomem *pll_base;
static struct clk *finclk;

static unsigned int max_support_idx;
static unsigned int min_support_idx = L12;
static unsigned int max_real_idx = L0;
static unsigned int min_real_idx = L14;	/* for thermal throttle */

static struct cpufreq_frequency_table sdp1404_big_freq_table[] = {
	{ L0, 1500*1000},
	{ L1, 1400*1000},
	{ L2, 1300*1000},
	{ L3, 1200*1000},	
	{ L4, 1100*1000},	
	{ L5, 1000*1000},
	{ L6,  900*1000},
	{ L7,  800*1000},
	{ L8,  700*1000},
	{ L9,  600*1000},
	{L10,  500*1000},
	{L11,  400*1000},
	{L12,  300*1000},
	{L13,  200*1000},
	{L14,  100*1000},
	{0, (u32)CPUFREQ_TABLE_END},
};

static unsigned int clkdiv_cpu[CPUFREQ_LEVEL_END] = {
	/* PMS value */
	0x40F400, /* 1500 L0 */
	0x40E400, /* 1400 L1 */
	0xA21100, /* 1300 L2 */
	0x624A01, /* 1200 L3 */
	0x416601, /* 1100 L4 */
	0x828B01, /* 1000 L5 */
	0x412501, /*  900 L6 */
	0xA28B01, /*  800 L7 */
	0x40E401, /*  700 L8 */
	0x624A02, /*  600 L9 */
	0x828B02, /*  500 L10 */
	0xA28B02, /*  400 L11 */
	0x624A03, /*  300 L12 */
	0xA28B03, /*  200 L13 */
	0xA28B04, /*  100 L14 */
};

/* voltage table (uV scale) */
static unsigned int sdp1404_big_volt_table[CPUFREQ_LEVEL_END];

static const unsigned int sdp1404_big_asv_voltage[CPUFREQ_LEVEL_END][CPUFREQ_ASV_COUNT] = {
	/*  ASV0,    ASV1,    ASV2,    ASV3,	ASV4,	 ASV5,	  ASV6,   ASV7,     ASV8,    ASV9 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L0 1500 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L1 1400 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L2 1300 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L3 1200 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L4 1100 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L5 1000 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L6  900 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L7  800 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L8  700 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L9  600 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L10 500 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L11 400 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L12 300 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L13 200 */
	{1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000, 1130000}, /* L14 100 */
};

static const unsigned int sdp1404_big_asv_voltage_evt1[CPUFREQ_LEVEL_END][CPUFREQ_ASV_COUNT] = {
	/*  ASV0,    ASV1,    ASV2,    ASV3,    ASV4,    ASV5,    ASV6,   ASV7,     ASV8,    ASV9 */
	{1230000, 1230000, 1160000, 1110000, 1100000, 1080000, 1060000, 1060000, 1060000, 1060000}, /* L0 1500 */
	{1230000, 1180000, 1130000, 1070000, 1060000, 1040000, 1020000, 1020000, 1020000, 1020000}, /* L1 1400 */
	{1230000, 1150000, 1090000, 1040000, 1020000, 1000000,  980000,  980000,  980000,  980000}, /* L2 1300 */
	{1230000, 1100000, 1070000, 1030000, 1010000,  990000,  970000,  970000,  970000,  970000}, /* L3 1200 */
	{1230000, 1070000, 1040000, 1010000,  990000,  980000,  960000,  960000,  960000,  960000}, /* L4 1100 */
	{1230000, 1030000, 1030000, 1000000,  980000,  970000,  950000,  950000,  950000,  950000}, /* L5 1000 */
	{1230000, 1020000, 1020000,  990000,  970000,  960000,  940000,  940000,  940000,  940000}, /* L6  900 */
	{1230000, 1010000, 1000000,  960000,  950000,  940000,  930000,  930000,  930000,  930000}, /* L7  800 */
	{1230000, 1000000,  990000,  950000,  940000,  930000,  920000,  920000,  920000,  920000}, /* L8  700 */
	{1230000,  990000,  980000,  950000,  940000,  920000,  920000,  920000,  920000,  920000}, /* L9  600 */
	{1230000,  980000,  970000,  940000,  930000,  920000,  920000,  920000,  920000,  920000}, /* L10 500 */
	{1230000,  970000,  960000,  930000,  920000,  920000,  920000,  920000,  920000,  920000}, /* L11 400 */
	{1230000,  950000,  950000,  920000,  920000,  920000,  920000,  920000,  920000,  920000}, /* L12 300 */
	{1230000,  950000,  950000,  920000,  920000,  920000,  920000,  920000,  920000,  920000}, /* L13 200 */
	{1230000,  950000,  950000,  920000,  920000,  920000,  920000,  920000,  920000,  920000}, /* L14 100 */
};

#define SDP1404_BG_PWM_CLK_CON		(SFR_VA + 0xF90008)
#define SEL_ARM_VS_AMS			(1 << 8)	/* clock change (0: AMS clock, 1: ARM clock) */
#define SEL_AMSHALF			(12)		/* AMS clock (0: 1000MHz, 1: 500MHz, 2:250MHz, 3: 125MHz) */
#define SEL_FTEST_AMS			(1 << 16)	/* select AMS clock for temp clock (0: 24MHz, 1: AMS) */
#define SDP1404_PLL_BASE		0x11250800
#define SDP1404_PLL_RESETN		(SDP1404_PLL_BASE + 0x9C)	/* pll power down (physical address) */
#define CPU_PLL				(1 << 17)
#define SDP1404_CPU_PLL_OFFSET		(0x0)		/* pll register (0x11250800 base offset) */
#define SDP1404_CPU_PLL_LOCKEN_OFFSET	(0x44)		/* pll lock enable */
#define SDP1404_PLL_LOCK_OFFSET		(0x94)		/* pll lock */
#define CPU_PLL_LOCK			(1 << 3)
#define CPUFREQ_TEMP_FREQ		(125000)

static unsigned int sdp1404_big_get_speed(unsigned int cpu)
{
	unsigned int ret;
	unsigned int pms;
	unsigned int infreq;

	if (!pll_base) {
		pr_err("error: big pll_base is NULL\n");
		return 0;
	}

	if (finclk)
		infreq = clk_get_rate(finclk);
	else
		infreq = 24576000UL;

	pms = readl((void *)((u32)pll_base + SDP1404_CPU_PLL_OFFSET));

	ret = (infreq >> (pms & 0x7)) / ((pms >> 20) & 0x3F);
	ret *= ((pms >> 8) & 0x3FF); 
	
	/* convert to 10MHz scale */
	ret = ((ret + 5000000) / 10000000) * 10000000;

	return ret;
}

static void set_volt_table(int result)
{
	unsigned int i;
	unsigned int freq;

	/* get current cpu clock */
	freq = sdp1404_big_get_speed(0);
	for (i = L0; i < CPUFREQ_LEVEL_END; i++) {
		if ((sdp1404_big_freq_table[i].frequency*1000) == freq)
			break;
	}

	if (i < CPUFREQ_LEVEL_END)
		max_support_idx = i;

	pr_info("DVFS: current big CPU clk = %dMHz, max support freq is %dMHz",
				freq/1000000, sdp1404_big_freq_table[i].frequency/1000);
	
	for (i = L0; i < max_real_idx; i++)
		sdp1404_big_freq_table[i].frequency = (u32)CPUFREQ_ENTRY_INVALID;
}

static void update_volt_table(int result)
{
	int i;
	int rev;

	pr_info("DVFS: big CPU voltage table is setted with asv group %d\n", result);

	rev = sdp_get_revision_id();
	pr_info("DVFS: use evt%d table\n", rev);

	if (result < CPUFREQ_ASV_COUNT) { 
		for (i = 0; i < CPUFREQ_LEVEL_END; i++) {
			if (rev)
				sdp1404_big_volt_table[i] = sdp1404_big_asv_voltage_evt1[i][result];
			else
				sdp1404_big_volt_table[i] = sdp1404_big_asv_voltage[i][result];
		}
	} else {
		pr_err("%s: asv table index error. %d\n", __func__, result);
	}
}

extern int sdp_set_clockgating(u32 phy_addr, u32 mask, u32 value);

#if defined(CONFIG_ARM_SDP1404_CPUFREQ_ACLK)
static void sdp1404_big_set_aclk(unsigned int new_index)
{
	u32 val, newval;
	unsigned int freq = sdp1404_big_freq_table[new_index].frequency;

	if (hawkp_revision_id == 0)
		return;

	val = readl((void *)SDP1404_BG_PWM_CLK_CON);
	
	newval = val & (~(3UL << 28));	/* 3:1 */
	if (freq <= 500000)
		newval |= (2UL << 28);	/* 1:1 */
	else if (freq <= 1000000)
		newval |= (1UL << 28);	/* 2:1 */

	//pr_err ("big aclk %ldkhz %08x %08x\n", freq, val, newval);
	if (newval != val) {
		writel(newval, (void *)SDP1404_BG_PWM_CLK_CON);
		readl_relaxed((void *)SDP1404_BG_PWM_CLK_CON);	/* flush */
	}
}
#endif

static void sdp1404_big_set_clkdiv(unsigned int old_index, unsigned int new_index)
{
	u32 val;
	int tmp_div;
	
	/* calculate temp clock */
	if (old_index > new_index) {
		if (sdp1404_big_freq_table[new_index].frequency > 1000000)
			tmp_div = 0;
		else if (sdp1404_big_freq_table[new_index].frequency > 500000)
			tmp_div = 1;
		else
			tmp_div = 2;
	} else {
		/*
		 * when frequency down case,
		 * temp freq selection algorithm is
		 * 3 - MSB(old freq / 100MHz)
		 */
		tmp_div = (int)(sdp1404_big_freq_table[old_index].frequency / CPUFREQ_TEMP_FREQ);
		tmp_div = 3 - (fls(tmp_div) - 1);
		if (tmp_div > 3 || tmp_div < 0)
			tmp_div = 3;
	}

	/* set the mux to selected ams clock(sel_ams_half) */
	val = readl((void *)SDP1404_BG_PWM_CLK_CON) & (u32)(~(0x3 << SEL_AMSHALF));
	val |= (u32)tmp_div << SEL_AMSHALF; /* AMS clk div */
	writel(val, (void *)SDP1404_BG_PWM_CLK_CON);

	/* change CPU clock source to Temp clock(sel_arm_ams) */
	val = readl((void *)SDP1404_BG_PWM_CLK_CON);
	val &= (u32)(~SEL_ARM_VS_AMS);
	writel(val, (void *)SDP1404_BG_PWM_CLK_CON);

	/* PWD off */
	sdp_set_clockgating(SDP1404_PLL_RESETN, CPU_PLL, 0);
	/* change CPU pll value */
	writel(clkdiv_cpu[new_index], (void *)((u32)pll_base + SDP1404_CPU_PLL_OFFSET));
	/* PWD on */
	sdp_set_clockgating(SDP1404_PLL_RESETN, CPU_PLL, CPU_PLL);

	/* wait PLL lock */
	while (!(readl((void *)((u32)pll_base + SDP1404_PLL_LOCK_OFFSET)) & CPU_PLL_LOCK));

	/* change CPU clock source to ARM PLL(sel_arm_ams) */
	val = readl((void *)SDP1404_BG_PWM_CLK_CON);
	val |= SEL_ARM_VS_AMS;
	writel(val, (void *)SDP1404_BG_PWM_CLK_CON);	
}

static void sdp1404_big_set_frequency(unsigned int cpu, unsigned int old_index,
				unsigned int new_index, unsigned int mux)
{
	unsigned long flags;
	
	/* Change the system clock divider values */
	if (sdp_cpufreq_print_on)
	printk("@$b%u\n", sdp1404_big_freq_table[new_index].frequency/10000);

	spin_lock_irqsave(&freq_lock, flags);

#if defined(CONFIG_ARM_SDP1404_CPUFREQ_ACLK)
	if (old_index > new_index)	/* goes faster */
		sdp1404_big_set_aclk(new_index);
#endif

	/* change cpu frequnecy */
	sdp1404_big_set_clkdiv(old_index, new_index);

#if defined(CONFIG_ARM_SDP1404_CPUFREQ_ACLK)
	if (old_index < new_index)	/* goes slower */
		sdp1404_big_set_aclk(new_index);
#endif

	spin_unlock_irqrestore(&freq_lock, flags);
}

int sdp1404_big_cpufreq_init(struct sdp_dvfs_info *info)
{
	u32 val;
	
	/* ioremap */
	pll_base = ioremap(SDP1404_PLL_BASE, 0x200);
	if (pll_base == NULL) {
		pr_err("arm pll ioremap failed\n");
		return -ENODEV;
	}

	/* select temp clock to ams clock(sel_ftest_ams) */
	val = readl((void *)SDP1404_BG_PWM_CLK_CON) | SEL_FTEST_AMS;
	writel(val, (void *)SDP1404_BG_PWM_CLK_CON);

	/* pll lock enable(pll_cpu0_locken) */
	val = readl((void *)((u32)pll_base + SDP1404_CPU_PLL_LOCKEN_OFFSET));
	val |= 1 << 18;
	writel(val, (void *)((u32)pll_base + SDP1404_CPU_PLL_LOCKEN_OFFSET));

	/* get fin clock */
	finclk = clk_get(NULL, "fin_pll");
	if (!finclk)
		pr_err("finclk get fail\n");
	
	/* set default AVS off table */	
	set_volt_table(info->cur_group);

	info->max_real_idx = max_real_idx;
	info->max_support_idx = max_support_idx;
	info->min_support_idx = min_support_idx;
	info->min_real_idx = min_real_idx;
	info->volt_table = sdp1404_big_volt_table;
	info->freq_table = sdp1404_big_freq_table;
	info->set_freq = sdp1404_big_set_frequency;
	info->update_volt_table = update_volt_table;
	info->get_speed = sdp1404_big_get_speed;

	/* get revision id, ACLKM is only activated when rev_id >= EVT1 */
	hawkp_revision_id = sdp_get_revision_id();
	pr_info("big_cpufreq: revision %d, %s ACLKM function.\n", hawkp_revision_id,
			hawkp_revision_id ? "use" : "not use");

	return 0;	
}
EXPORT_SYMBOL(sdp1404_big_cpufreq_init);