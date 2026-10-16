// SPDX-License-Identifier: GPL-2.0-only
/*
 * portov diagnostic no-op reset controller.
 *
 * Motorola portov (SM7435/parrot) has no mainline GCC or reset driver yet,
 * but the mainline USB2 HS PHY driver (phy-qcom-snps-femto-v2) refuses to
 * probe without an exclusive reset controller.  This shim supplies one so the
 * USB2 PHY, and therefore the whole DWC3 path, can be exercised on hardware
 * before a real SM7435 reset binding exists.
 *
 * It performs no register access at all.  The bring-up image deliberately
 * relies on the bootloader having left the PHY powered and clocked, which is
 * exactly the handoff assumption under test.  The PHY driver still runs its
 * full register init sequence, so a no-op reset only means the PHY is not
 * power-on-reset again.
 *
 * This is a diagnostic-only helper.  It is not intended for upstream.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>

struct portov_diag_reset {
	struct reset_controller_dev rcdev;
};

static int portov_diag_reset_assert(struct reset_controller_dev *rcdev,
				    unsigned long id)
{
	return 0;
}

static int portov_diag_reset_deassert(struct reset_controller_dev *rcdev,
				      unsigned long id)
{
	return 0;
}

static int portov_diag_reset_reset(struct reset_controller_dev *rcdev,
				   unsigned long id)
{
	return 0;
}

static const struct reset_control_ops portov_diag_reset_ops = {
	.assert		= portov_diag_reset_assert,
	.deassert	= portov_diag_reset_deassert,
	.reset		= portov_diag_reset_reset,
};

static int portov_diag_reset_probe(struct platform_device *pdev)
{
	struct portov_diag_reset *dr;

	dr = devm_kzalloc(&pdev->dev, sizeof(*dr), GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	dr->rcdev.owner = THIS_MODULE;
	dr->rcdev.ops = &portov_diag_reset_ops;
	dr->rcdev.of_node = pdev->dev.of_node;
	dr->rcdev.nr_resets = 1;

	return devm_reset_controller_register(&pdev->dev, &dr->rcdev);
}

static const struct of_device_id portov_diag_reset_of_match[] = {
	{ .compatible = "portov,diag-noop-reset" },
	{ }
};
MODULE_DEVICE_TABLE(of, portov_diag_reset_of_match);

static struct platform_driver portov_diag_reset_driver = {
	.probe	= portov_diag_reset_probe,
	.driver	= {
		.name		= "portov-diag-reset",
		.of_match_table	= portov_diag_reset_of_match,
	},
};
builtin_platform_driver(portov_diag_reset_driver);
