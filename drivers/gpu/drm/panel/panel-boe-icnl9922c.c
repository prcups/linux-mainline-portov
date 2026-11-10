// SPDX-License-Identifier: GPL-2.0-only
/*
 * BOE/CSOT ICNL9922C 1080x2400 DSC video-mode panel
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/irqflags.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/preempt.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/timekeeping.h>

#include <video/mipi_display.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

#include "../msm/dsi/dsi_portov_diag.h"

struct icnl9922c {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data *supplies;
};

/*
 * Diagnostic gate for the first display bring-up.  Keep the panel probe
 * deferred until the initramfs shell is available, then clear this parameter
 * and manually trigger the probe so the USB log survives the panel handoff.
 */
static bool hold_probe = true;
module_param(hold_probe, bool, 0644);
MODULE_PARM_DESC(hold_probe, "Keep the ICNL9922C panel probe deferred");

static bool trace_power_mode;
module_param(trace_power_mode, bool, 0644);
MODULE_PARM_DESC(trace_power_mode, "Read DCS power mode after panel init");

static bool trace_reset;
module_param(trace_reset, bool, 0644);
MODULE_PARM_DESC(trace_reset, "Log portov panel prepare/reset boundaries");
static unsigned int reset_stage;
module_param(reset_stage, uint, 0444);
static int reset_task_pid;
module_param(reset_task_pid, int, 0444);
MODULE_PARM_DESC(reset_task_pid, "Task executing the traced panel reset");

static bool reset_tail_delay;
module_param(reset_tail_delay, bool, 0644);
MODULE_PARM_DESC(reset_tail_delay, "Use mdelay for the final portov reset wait");

static bool reset_tail_poll;
module_param(reset_tail_poll, bool, 0644);
MODULE_PARM_DESC(reset_tail_poll, "Use bounded polling for the final reset wait");

static unsigned int prepare_pause_secs;
module_param(prepare_pause_secs, uint, 0644);
MODULE_PARM_DESC(prepare_pause_secs, "Pause before panel supplies/reset, capped at 30s");
static unsigned int pause_reset_stage;
module_param(pause_reset_stage, uint, 0644);
MODULE_PARM_DESC(pause_reset_stage, "Pause at prepare entry (0) or reset stage 4/6/8");
static bool prepare_pause_active;
module_param(prepare_pause_active, bool, 0444);

static const struct regulator_bulk_data icnl9922c_supplies[] = {
	{ .supply = "vddio" },
	{ .supply = "lab" },
	{ .supply = "ibb" },
};

static inline struct icnl9922c *to_icnl9922c(struct drm_panel *panel)
{
	return container_of(panel, struct icnl9922c, panel);
}

static void icnl9922c_wait_mark(struct icnl9922c *ctx, unsigned int stage)
{
	unsigned long hard = 0, timer = 0, hrtimer = 0;
	int cpu;

	/* Approximate lockless system-wide counts; sampled CPUs may race. */
	for_each_online_cpu(cpu) {
		hard += READ_ONCE(kstat_cpu(cpu).irqs_sum);
		timer += READ_ONCE(kstat_cpu(cpu).softirqs[TIMER_SOFTIRQ]);
		hrtimer += READ_ONCE(kstat_cpu(cpu).softirqs[HRTIMER_SOFTIRQ]);
	}

	dev_info(&ctx->dsi->dev,
		 "PORTOV_PANEL_WAIT: stage=%u delay=%u cpu=%u j=%lu mono_ns=%llu hard=%lu timer=%lu hrtimer=%lu irqoff=%u preempt=%x\n",
		 stage, (unsigned int)READ_ONCE(reset_tail_delay),
		 raw_smp_processor_id(), READ_ONCE(jiffies),
		 (unsigned long long)ktime_get_mono_fast_ns(),
		 hard, timer, hrtimer, irqs_disabled() ? 1U : 0U, preempt_count());
}

static void icnl9922c_reset_mark(struct icnl9922c *ctx, unsigned int stage)
{
	if (!READ_ONCE(trace_reset) ||
	    !of_machine_is_compatible("motorola,portov"))
		return;

	if (stage == 1)
		WRITE_ONCE(reset_task_pid, task_pid_nr(current));
	WRITE_ONCE(reset_stage, stage);
	dev_info(&ctx->dsi->dev, "PORTOV_PANEL_RESET: stage=%u\n", stage);
	msm_dsi_host_portov_panel_trace(ctx->dsi->host, stage);
	if (stage == 8 || stage == 9)
		icnl9922c_wait_mark(ctx, stage);
}

/* No sleep, cpu_relax, WFE or WFIT in this diagnostic loop. IRQs stay enabled. */
static int icnl9922c_poll_tail(struct icnl9922c *ctx)
{
	u64 start = ktime_get_mono_fast_ns();
	unsigned int i;

	for (i = 0; i < 2000000; i++) {
		if (ktime_get_mono_fast_ns() - start >= 10000000ULL) {
			dev_info(&ctx->dsi->dev,
				 "PORTOV_PANEL_POLL: complete iterations=%u\n", i);
			return 0;
		}
		barrier();
	}

	dev_err(&ctx->dsi->dev, "PORTOV_PANEL_POLL: iteration limit\n");
	return -ETIMEDOUT;
}

static void icnl9922c_prepare_pause(struct icnl9922c *ctx, unsigned int stage);

static int icnl9922c_reset(struct icnl9922c *ctx)
{
	icnl9922c_reset_mark(ctx, 3);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	icnl9922c_reset_mark(ctx, 4);
	icnl9922c_prepare_pause(ctx, 4);
	usleep_range(10000, 11000);
	icnl9922c_reset_mark(ctx, 5);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	icnl9922c_reset_mark(ctx, 6);
	icnl9922c_prepare_pause(ctx, 6);
	usleep_range(2000, 3000);
	icnl9922c_reset_mark(ctx, 7);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	icnl9922c_reset_mark(ctx, 8);
	icnl9922c_prepare_pause(ctx, 8);
	if (READ_ONCE(reset_tail_poll) &&
	    of_machine_is_compatible("motorola,portov")) {
		int ret;

		dev_info(&ctx->dsi->dev, "PORTOV_PANEL_POLL: begin\n");
		ret = icnl9922c_poll_tail(ctx);
		if (ret)
			return ret;
	} else if (READ_ONCE(reset_tail_delay) &&
	    of_machine_is_compatible("motorola,portov"))
		mdelay(10);
	else
		usleep_range(10000, 11000);
	icnl9922c_reset_mark(ctx, 9);
	return 0;
}

static void icnl9922c_step_result(struct icnl9922c *ctx,
				 struct mipi_dsi_multi_context *multi,
				 const char *step)
{
	dev_info(&ctx->dsi->dev, "PORTOV_PANEL: %s result=%d (negative=failed/skipped)\n",
		 step, multi->accum_err);
}

static int icnl9922c_on(struct icnl9922c *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };
	struct drm_dsc_picture_parameter_set pps;

	dev_info(&ctx->dsi->dev, "PORTOV_PANEL: DCS init start\n");

	dev_info(&ctx->dsi->dev, "PORTOV_PANEL: cmd f0 start\n");
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x99, 0x22, 0x0c);
	icnl9922c_step_result(ctx, &dsi_ctx, "cmd f0");
	dev_info(&ctx->dsi->dev, "PORTOV_PANEL: cmd c1 start\n");
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1,
		0x00, 0x00, 0x20, 0x26, 0x26, 0x04, 0x20, 0x20,
		0x04, 0x60, 0x19, 0x22, 0x50, 0x01, 0x12, 0x07,
		0x5f, 0xff, 0xa0, 0x00, 0x93, 0x10, 0x10, 0x00);
	icnl9922c_step_result(ctx, &dsi_ctx, "cmd c1");
	dev_info(&ctx->dsi->dev, "PORTOV_PANEL: cmd e0 start\n");
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe0,
		0x0c, 0x00, 0xb0, 0x0c, 0x00, 0x15, 0x7c, 0x29,
		0x04, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2e,
		0x17, 0x0c, 0x18);
	icnl9922c_step_result(ctx, &dsi_ctx, "cmd e0");
	dev_info(&ctx->dsi->dev, "PORTOV_PANEL: parameter commands start\n");
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x35, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx,
				     MIPI_DCS_SET_DISPLAY_BRIGHTNESS,
				     0x07, 0xff);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x53, 0x2c, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x55, 0x03, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 2);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x00, 0x00, 0x00);
	icnl9922c_step_result(ctx, &dsi_ctx, "parameter commands");
	icnl9922c_step_result(ctx, &dsi_ctx, "DCS init");

	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	icnl9922c_step_result(ctx, &dsi_ctx, "sleep out");
	mipi_dsi_msleep(&dsi_ctx, 90);

	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xac, 0x05, 0x00);
	icnl9922c_step_result(ctx, &dsi_ctx, "display on");

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);
	mipi_dsi_picture_parameter_set_multi(&dsi_ctx, &pps);
	icnl9922c_step_result(ctx, &dsi_ctx, "PPS");

	if (READ_ONCE(trace_power_mode) && !dsi_ctx.accum_err) {
		u8 mode = 0;
		int ret = mipi_dsi_dcs_get_power_mode(ctx->dsi, &mode);

		dev_info(&ctx->dsi->dev,
			 "PORTOV_PANEL_POWER_MODE: ret=%d mode=%#x\n", ret, mode);
	}

	return dsi_ctx.accum_err;
}

static int icnl9922c_off(struct icnl9922c *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x99, 0x22, 0x0c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1,
		0x00, 0x00, 0x20, 0x26, 0x26, 0x04, 0x20, 0x20,
		0x04, 0x60, 0x19, 0x22, 0x50, 0x01, 0x12, 0x07,
		0x5f, 0xff, 0xa0, 0x00, 0x93, 0x10, 0x10, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xac, 0x0a, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 20);
	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 100);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);

	return dsi_ctx.accum_err;
}

/* Diagnostic pause: snapshot once, with a finite number of sleep calls. */
static void icnl9922c_prepare_pause(struct icnl9922c *ctx, unsigned int stage)
{
	unsigned int seconds, i;

	if (!of_machine_is_compatible("motorola,portov"))
		return;
	if (stage != READ_ONCE(pause_reset_stage))
		return;
	seconds = min(READ_ONCE(prepare_pause_secs), 30U);
	if (!seconds)
		return;

	if (READ_ONCE(trace_reset))
		WRITE_ONCE(reset_task_pid, task_pid_nr(current));
	WRITE_ONCE(prepare_pause_active, true);
	dev_info(&ctx->dsi->dev, "PORTOV_PREPARE_PAUSE: begin seconds=%u\n", seconds);
	for (i = 0; i < seconds; i++) {
		msleep(1000);
		dev_info(&ctx->dsi->dev, "PORTOV_PREPARE_PAUSE: elapsed=%u\n", i + 1);
	}
	WRITE_ONCE(prepare_pause_active, false);
	dev_info(&ctx->dsi->dev, "PORTOV_PREPARE_PAUSE: released\n");
}

static int icnl9922c_prepare(struct drm_panel *panel)
{
	struct icnl9922c *ctx = to_icnl9922c(panel);
	int ret;

	dev_info(panel->dev, "PORTOV_PANEL: prepare start\n");
	icnl9922c_prepare_pause(ctx, 0);
	ret = regulator_bulk_enable(ARRAY_SIZE(icnl9922c_supplies),
				    ctx->supplies);
	if (ret)
		return ret;
	dev_info(panel->dev, "PORTOV_PANEL: supplies enabled\n");

	icnl9922c_reset_mark(ctx, 1);
	usleep_range(3000, 4000);
	icnl9922c_reset_mark(ctx, 2);
	ret = icnl9922c_reset(ctx);
	if (ret) {
		regulator_bulk_disable(ARRAY_SIZE(icnl9922c_supplies),
				       ctx->supplies);
		return ret;
	}
	dev_info(panel->dev, "PORTOV_PANEL: reset sequence complete\n");

	icnl9922c_reset_mark(ctx, 10);
	ret = icnl9922c_on(ctx);
	if (ret) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		regulator_bulk_disable(ARRAY_SIZE(icnl9922c_supplies),
				       ctx->supplies);
	}
	else {
		dev_info(panel->dev, "PORTOV_PANEL: prepare complete\n");
		msm_dsi_host_portov_panel_ready(ctx->dsi->host);
	}

	return ret;
}

static int icnl9922c_unprepare(struct drm_panel *panel)
{
	struct icnl9922c *ctx = to_icnl9922c(panel);
	int ret;

	ret = icnl9922c_off(ctx);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	regulator_bulk_disable(ARRAY_SIZE(icnl9922c_supplies),
			       ctx->supplies);

	return ret;
}

static const struct drm_display_mode icnl9922c_mode = {
	.clock = (1080 + 32 + 4 + 32) * (2400 + 38 + 4 + 32) * 120 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 32,
	.hsync_end = 1080 + 32 + 4,
	.htotal = 1080 + 32 + 4 + 32,
	.vdisplay = 2400,
	.vsync_start = 2400 + 38,
	.vsync_end = 2400 + 38 + 4,
	.vtotal = 2400 + 38 + 4 + 32,
	.width_mm = 70,
	.height_mm = 156,
	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int icnl9922c_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector,
						    &icnl9922c_mode);
}

static const struct drm_panel_funcs icnl9922c_panel_funcs = {
	.prepare = icnl9922c_prepare,
	.unprepare = icnl9922c_unprepare,
	.get_modes = icnl9922c_get_modes,
};

static int icnl9922c_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	ret = mipi_dsi_dcs_set_display_brightness_large(dsi, brightness);

	return ret;
}

static const struct backlight_ops icnl9922c_bl_ops = {
	.update_status = icnl9922c_bl_update_status,
};

static struct backlight_device *
icnl9922c_create_backlight(struct mipi_dsi_device *dsi)
{
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 0x4e3,
		.max_brightness = 0x7ff,
	};

	return devm_backlight_device_register(&dsi->dev, dev_name(&dsi->dev),
					      &dsi->dev, dsi,
					      &icnl9922c_bl_ops, &props);
}

static int icnl9922c_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct icnl9922c *ctx;
	int ret;

	if (hold_probe)
		return -EPROBE_DEFER;

	ctx = devm_drm_panel_alloc(dev, struct icnl9922c, panel,
				   &icnl9922c_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ret = devm_regulator_bulk_get_const(dev, ARRAY_SIZE(icnl9922c_supplies),
					    icnl9922c_supplies,
					    &ctx->supplies);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get supplies\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "failed to get reset gpio\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dsc.dsc_version_major = 0x1;
	ctx->dsc.dsc_version_minor = 0x1;
	ctx->dsc.slice_height = 12;
	ctx->dsc.slice_width = 540;
	ctx->dsc.slice_count = 2;
	ctx->dsc.bits_per_component = 8;
	ctx->dsc.bits_per_pixel = 8 << 4;
	ctx->dsc.block_pred_enable = true;

	dsi->dsc = &ctx->dsc;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM |
			  MIPI_DSI_MODE_DSC_ALL_SLICES_IN_PKT;

	ctx->panel.prepare_prev_first = true;
	ctx->panel.backlight = icnl9922c_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "failed to create backlight\n");

	ret = devm_drm_panel_add(dev, &ctx->panel);
	if (ret)
		return ret;

	return devm_mipi_dsi_attach(dev, dsi);
}

static const struct of_device_id icnl9922c_of_match[] = {
	{ .compatible = "boe,icnl9922c" },
	{ .compatible = "csot,icnl9922c" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, icnl9922c_of_match);

static struct mipi_dsi_driver icnl9922c_driver = {
	.probe = icnl9922c_probe,
	.driver = {
		.name = "panel-boe-icnl9922c",
		.of_match_table = icnl9922c_of_match,
	},
};
module_mipi_dsi_driver(icnl9922c_driver);

MODULE_DESCRIPTION("BOE/CSOT ICNL9922C MIPI DSI panel");
MODULE_LICENSE("GPL");
