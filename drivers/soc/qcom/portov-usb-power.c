// SPDX-License-Identifier: GPL-2.0-only
/*
 * portov diagnostic USB power bridge.
 *
 * Motorola portov (SM7435/parrot) has no mainline GCC driver, so nothing in a
 * mainline boot enables the USB controller's clocks or its USB3 GDSC power
 * domain.  The USB-handoff and USB-PHY diagnostic images both held with no
 * CDC ACM gadget, which is consistent with the DWC3/QSCRATCH register block
 * being inaccessible.
 *
 * This bridge runs before dwc3-qcom probes and directly programs the few GCC
 * registers needed to power the USB controller and its HS PHY.  Offsets are
 * taken from the downstream clock driver drivers/clk/qcom/gcc-parrot.c and
 * its reset map (LineageOS android_kernel_motorola_sm7435):
 *
 *   GCC_USB30_PRIM_BCR		0x49000	(reset index 18)
 *   GCC_QUSB2PHY_PRIM_BCR	0x22000	(reset index 14)
 *   gcc_usb30_prim_gdsc		0x49004
 *   gcc_usb30_prim_master_clk	0x49018
 *   gcc_usb30_prim_sleep_clk	0x49020
 *   gcc_usb30_prim_mock_utmi_clk	0x49024
 *   gcc_usb30_prim_master_clk_src	0x49028
 *   gcc_usb30_prim_mock_utmi_clk_src	0x49040
 *   gcc_usb3_prim_phy_aux_clk	0x4905c
 *   gcc_usb3_prim_phy_com_aux_clk	0x49060
 *   gcc_usb3_prim_phy_aux_clk_src	0x4906c
 *   gcc_cfg_noc_usb3_prim_axi_clk	0x49084
 *   gcc_aggre_usb3_prim_axi_clk	0x49088
 *   gcc_usb3_0_clkref_en		0x9c010
 *
 * It only enables branches and roots and deasserts resets; it does not
 * reconfigure source selection or dividers, so a controller already set up by
 * the bootloader keeps that configuration.  This is diagnostic-only and is
 * not intended for upstream: the real fix is a proper SM7435 GCC driver with
 * clock, reset and power-domain support.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>

#define PORTOV_GCC_USB30_PRIM_BCR		0x49000
#define PORTOV_GCC_QUSB2PHY_PRIM_BCR		0x22000
#define PORTOV_GCC_USB30_PRIM_GDSCR		0x49004
#define PORTOV_GCC_USB30_PRIM_MASTER_CLK	0x49018
#define PORTOV_GCC_USB30_PRIM_SLEEP_CLK		0x49020
#define PORTOV_GCC_USB30_PRIM_MOCK_UTMI_CLK	0x49024
#define PORTOV_GCC_USB30_PRIM_MASTER_CLK_SRC	0x49028
#define PORTOV_GCC_USB30_PRIM_MOCK_UTMI_SRC	0x49040
#define PORTOV_GCC_USB3_PRIM_PHY_AUX_CLK	0x4905c
#define PORTOV_GCC_USB3_PRIM_PHY_COM_AUX_CLK	0x49060
#define PORTOV_GCC_USB3_PRIM_PHY_AUX_CLK_SRC	0x4906c
#define PORTOV_GCC_CFG_NOC_USB3_PRIM_AXI_CLK	0x49084
#define PORTOV_GCC_AGGRE_USB3_PRIM_AXI_CLK	0x49088
#define PORTOV_GCC_USB3_0_CLKREF_EN		0x9c010

/* drivers/clk/qcom/gdsc.c */
#define GDSC_SW_COLLAPSE	BIT(0)
#define GDSC_SW_OVERRIDE	BIT(2)
#define GDSC_RETAIN_FF_ENABLE	BIT(11)
#define GDSC_PWR_ON		BIT(31)

/* drivers/clk/qcom/clk-branch.c: enable bit 0, halt status bit 31. */
#define CBCR_ENABLE		BIT(0)
#define CBCR_HALT		BIT(31)

/* drivers/clk/qcom/clk-rcg2.c: root enable is bit 1 of CMD_RCGR. */
#define RCG_CMD_ROOT_EN		BIT(1)

/* BCR: bit 0 set asserts the block reset. */
#define BCR_ASSERT		BIT(0)

static const u32 portov_usb_bcrs[] = {
	PORTOV_GCC_USB30_PRIM_BCR,
	PORTOV_GCC_QUSB2PHY_PRIM_BCR,
};

static const u32 portov_usb_branches[] = {
	PORTOV_GCC_USB30_PRIM_MASTER_CLK,
	PORTOV_GCC_USB30_PRIM_SLEEP_CLK,
	PORTOV_GCC_USB30_PRIM_MOCK_UTMI_CLK,
	PORTOV_GCC_USB3_PRIM_PHY_AUX_CLK,
	PORTOV_GCC_USB3_PRIM_PHY_COM_AUX_CLK,
	PORTOV_GCC_CFG_NOC_USB3_PRIM_AXI_CLK,
	PORTOV_GCC_AGGRE_USB3_PRIM_AXI_CLK,
	PORTOV_GCC_USB3_0_CLKREF_EN,
};

static const u32 portov_usb_rcgs[] = {
	PORTOV_GCC_USB30_PRIM_MASTER_CLK_SRC,
	PORTOV_GCC_USB30_PRIM_MOCK_UTMI_SRC,
	PORTOV_GCC_USB3_PRIM_PHY_AUX_CLK_SRC,
};

static void __iomem *portov_gcc_base;

static void portov_gdsc_on(void __iomem *base)
{
	u32 val;
	int i;

	val = readl(base + PORTOV_GCC_USB30_PRIM_GDSCR);
	val |= GDSC_SW_OVERRIDE | GDSC_RETAIN_FF_ENABLE;
	val &= ~GDSC_SW_COLLAPSE;
	writel(val, base + PORTOV_GCC_USB30_PRIM_GDSCR);

	for (i = 0; i < 1000; i++) {
		if (readl(base + PORTOV_GCC_USB30_PRIM_GDSCR) & GDSC_PWR_ON)
			return;
		udelay(1);
	}

	pr_warn("portov-usb-power: USB3 GDSC did not report PWR_ON\n");
}

static void portov_resets_deassert(void __iomem *base)
{
	u32 val;
	int i;

	for (i = 0; i < ARRAY_SIZE(portov_usb_bcrs); i++) {
		val = readl(base + portov_usb_bcrs[i]);
		writel(val & ~BCR_ASSERT, base + portov_usb_bcrs[i]);
	}

	/* The BCRs need a moment before the blocks accept accesses. */
	udelay(10);
}

static void portov_clocks_enable(void __iomem *base)
{
	u32 val;
	int i;

	for (i = 0; i < ARRAY_SIZE(portov_usb_rcgs); i++) {
		val = readl(base + portov_usb_rcgs[i]);
		writel(val | RCG_CMD_ROOT_EN, base + portov_usb_rcgs[i]);
	}

	for (i = 0; i < ARRAY_SIZE(portov_usb_branches); i++) {
		val = readl(base + portov_usb_branches[i]);
		writel(val | CBCR_ENABLE, base + portov_usb_branches[i]);
	}

	for (i = 0; i < ARRAY_SIZE(portov_usb_branches); i++) {
		int poll;

		for (poll = 0; poll < 1000; poll++) {
			if (!(readl(base + portov_usb_branches[i]) & CBCR_HALT))
				break;
			udelay(1);
		}
		if (poll == 1000)
			pr_warn("portov-usb-power: clock at +0x%05x still halted\n",
				portov_usb_branches[i]);
	}
}

static int __init portov_usb_power_bridge(void)
{
	struct device_node *node;
	struct resource res;
	int ret;

	node = of_find_compatible_node(NULL, NULL, "qcom,parrot-gcc");
	if (!node) {
		pr_warn("portov-usb-power: no qcom,parrot-gcc node; bridge idle\n");
		return 0;
	}

	ret = of_address_to_resource(node, 0, &res);
	of_node_put(node);
	if (ret) {
		pr_warn("portov-usb-power: GCC node has no usable reg\n");
		return 0;
	}

	portov_gcc_base = ioremap(res.start, resource_size(&res));
	if (!portov_gcc_base)
		return -ENOMEM;

	pr_info("portov-usb-power: bridging SM7435 USB power at %pa\n", &res.start);

	portov_gdsc_on(portov_gcc_base);
	portov_resets_deassert(portov_gcc_base);
	portov_clocks_enable(portov_gcc_base);

	/*
	 * Keep the mapping: the object is to leave the controller powered when
	 * dwc3-qcom probes, not to model the clocks.
	 */
	return 0;
}

/*
 * Must run before dwc3-qcom (device_initcall) and before the deferred-probe
 * loop gives up, so the platform device is already powered when its driver
 * binds.
 */
postcore_initcall(portov_usb_power_bridge);
