// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2026 portov mainline contributors
 *
 * SM7435 (Qualcomm codename parrot) TLMM.  The register layout and the pin
 * list come from the downstream pinctrl-parrot driver; the shape of the file
 * follows the upstream SM4450 driver, which is the same silicon family.
 *
 * This first step describes the pins, the pin groups and the GPIO controller
 * only.  It deliberately carries no pin-function table yet, so pin muxing
 * (UART/I2C/SPI functions) is not available: any device tree state that names
 * a function cannot be applied until the function tables are ported and
 * normalised.  Configuration-only states - bias, drive strength, GPIO
 * direction - work, which is what the GPIO consumers on this board need.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "pinctrl-msm.h"

#define REG_SIZE 0x1000

#define PINGROUP(id)					\
	{						\
		.grp = PINCTRL_PINGROUP("gpio" #id,	\
			gpio##id##_pins,		\
			ARRAY_SIZE(gpio##id##_pins)),	\
		.funcs = (int[]){			\
			msm_mux_gpio,			\
		},					\
		.nfuncs = 1,				\
		.ctl_reg = REG_SIZE * id,		\
		.io_reg = 0x4 + REG_SIZE * id,		\
		.intr_cfg_reg = 0x8 + REG_SIZE * id,	\
		.intr_status_reg = 0xc + REG_SIZE * id,	\
		.mux_bit = 2,				\
		.pull_bit = 0,				\
		.drv_bit = 6,				\
		.egpio_enable = 12,			\
		.egpio_present = 11,			\
		.oe_bit = 9,				\
		.in_bit = 0,				\
		.out_bit = 1,				\
		.intr_enable_bit = 0,			\
		.intr_status_bit = 0,			\
		.intr_target_bit = 5,			\
		.intr_target_kpss_val = 3,		\
		.intr_raw_status_bit = 4,		\
		.intr_polarity_bit = 1,			\
		.intr_detection_bit = 2,		\
		.intr_detection_width = 2,		\
	}

#define SDC_QDSD_PINGROUP(pg_name, ctl, pull, drv)	\
	{					        \
		.grp = PINCTRL_PINGROUP(#pg_name,       \
			pg_name##_pins,                 \
			ARRAY_SIZE(pg_name##_pins)),    \
		.ctl_reg = ctl,				\
		.io_reg = 0,				\
		.intr_cfg_reg = 0,			\
		.intr_status_reg = 0,			\
		.mux_bit = -1,				\
		.pull_bit = pull,			\
		.drv_bit = drv,				\
		.oe_bit = -1,				\
		.in_bit = -1,				\
		.out_bit = -1,				\
		.intr_enable_bit = -1,			\
		.intr_status_bit = -1,			\
		.intr_target_bit = -1,			\
		.intr_raw_status_bit = -1,		\
		.intr_polarity_bit = -1,		\
		.intr_detection_bit = -1,		\
		.intr_detection_width = -1,		\
	}

#define UFS_RESET(pg_name, offset)				\
	{					        \
		.grp = PINCTRL_PINGROUP(#pg_name,       \
			pg_name##_pins,                 \
			ARRAY_SIZE(pg_name##_pins)),    \
		.ctl_reg = offset,			\
		.io_reg = offset + 0x4,			\
		.intr_cfg_reg = 0,			\
		.intr_status_reg = 0,			\
		.mux_bit = -1,				\
		.pull_bit = 3,				\
		.drv_bit = 0,				\
		.oe_bit = -1,				\
		.in_bit = -1,				\
		.out_bit = 0,				\
		.intr_enable_bit = -1,			\
		.intr_status_bit = -1,			\
		.intr_target_bit = -1,			\
		.intr_raw_status_bit = -1,		\
		.intr_polarity_bit = -1,		\
		.intr_detection_bit = -1,		\
		.intr_detection_width = -1,		\
	}

enum {
	msm_mux_gpio,
};

static const struct pinctrl_pin_desc sm7435_pins[] = {
	PINCTRL_PIN(0, "gpio0"),
	PINCTRL_PIN(1, "gpio1"),
	PINCTRL_PIN(2, "gpio2"),
	PINCTRL_PIN(3, "gpio3"),
	PINCTRL_PIN(4, "gpio4"),
	PINCTRL_PIN(5, "gpio5"),
	PINCTRL_PIN(6, "gpio6"),
	PINCTRL_PIN(7, "gpio7"),
	PINCTRL_PIN(8, "gpio8"),
	PINCTRL_PIN(9, "gpio9"),
	PINCTRL_PIN(10, "gpio10"),
	PINCTRL_PIN(11, "gpio11"),
	PINCTRL_PIN(12, "gpio12"),
	PINCTRL_PIN(13, "gpio13"),
	PINCTRL_PIN(14, "gpio14"),
	PINCTRL_PIN(15, "gpio15"),
	PINCTRL_PIN(16, "gpio16"),
	PINCTRL_PIN(17, "gpio17"),
	PINCTRL_PIN(18, "gpio18"),
	PINCTRL_PIN(19, "gpio19"),
	PINCTRL_PIN(20, "gpio20"),
	PINCTRL_PIN(21, "gpio21"),
	PINCTRL_PIN(22, "gpio22"),
	PINCTRL_PIN(23, "gpio23"),
	PINCTRL_PIN(24, "gpio24"),
	PINCTRL_PIN(25, "gpio25"),
	PINCTRL_PIN(26, "gpio26"),
	PINCTRL_PIN(27, "gpio27"),
	PINCTRL_PIN(28, "gpio28"),
	PINCTRL_PIN(29, "gpio29"),
	PINCTRL_PIN(30, "gpio30"),
	PINCTRL_PIN(31, "gpio31"),
	PINCTRL_PIN(32, "gpio32"),
	PINCTRL_PIN(33, "gpio33"),
	PINCTRL_PIN(34, "gpio34"),
	PINCTRL_PIN(35, "gpio35"),
	PINCTRL_PIN(36, "gpio36"),
	PINCTRL_PIN(37, "gpio37"),
	PINCTRL_PIN(38, "gpio38"),
	PINCTRL_PIN(39, "gpio39"),
	PINCTRL_PIN(40, "gpio40"),
	PINCTRL_PIN(41, "gpio41"),
	PINCTRL_PIN(42, "gpio42"),
	PINCTRL_PIN(43, "gpio43"),
	PINCTRL_PIN(44, "gpio44"),
	PINCTRL_PIN(45, "gpio45"),
	PINCTRL_PIN(46, "gpio46"),
	PINCTRL_PIN(47, "gpio47"),
	PINCTRL_PIN(48, "gpio48"),
	PINCTRL_PIN(49, "gpio49"),
	PINCTRL_PIN(50, "gpio50"),
	PINCTRL_PIN(51, "gpio51"),
	PINCTRL_PIN(52, "gpio52"),
	PINCTRL_PIN(53, "gpio53"),
	PINCTRL_PIN(54, "gpio54"),
	PINCTRL_PIN(55, "gpio55"),
	PINCTRL_PIN(56, "gpio56"),
	PINCTRL_PIN(57, "gpio57"),
	PINCTRL_PIN(58, "gpio58"),
	PINCTRL_PIN(59, "gpio59"),
	PINCTRL_PIN(60, "gpio60"),
	PINCTRL_PIN(61, "gpio61"),
	PINCTRL_PIN(62, "gpio62"),
	PINCTRL_PIN(63, "gpio63"),
	PINCTRL_PIN(64, "gpio64"),
	PINCTRL_PIN(65, "gpio65"),
	PINCTRL_PIN(66, "gpio66"),
	PINCTRL_PIN(67, "gpio67"),
	PINCTRL_PIN(68, "gpio68"),
	PINCTRL_PIN(69, "gpio69"),
	PINCTRL_PIN(70, "gpio70"),
	PINCTRL_PIN(71, "gpio71"),
	PINCTRL_PIN(72, "gpio72"),
	PINCTRL_PIN(73, "gpio73"),
	PINCTRL_PIN(74, "gpio74"),
	PINCTRL_PIN(75, "gpio75"),
	PINCTRL_PIN(76, "gpio76"),
	PINCTRL_PIN(77, "gpio77"),
	PINCTRL_PIN(78, "gpio78"),
	PINCTRL_PIN(79, "gpio79"),
	PINCTRL_PIN(80, "gpio80"),
	PINCTRL_PIN(81, "gpio81"),
	PINCTRL_PIN(82, "gpio82"),
	PINCTRL_PIN(83, "gpio83"),
	PINCTRL_PIN(84, "gpio84"),
	PINCTRL_PIN(85, "gpio85"),
	PINCTRL_PIN(86, "gpio86"),
	PINCTRL_PIN(87, "gpio87"),
	PINCTRL_PIN(88, "gpio88"),
	PINCTRL_PIN(89, "gpio89"),
	PINCTRL_PIN(90, "gpio90"),
	PINCTRL_PIN(91, "gpio91"),
	PINCTRL_PIN(92, "gpio92"),
	PINCTRL_PIN(93, "gpio93"),
	PINCTRL_PIN(94, "gpio94"),
	PINCTRL_PIN(95, "gpio95"),
	PINCTRL_PIN(96, "gpio96"),
	PINCTRL_PIN(97, "gpio97"),
	PINCTRL_PIN(98, "gpio98"),
	PINCTRL_PIN(99, "gpio99"),
	PINCTRL_PIN(100, "gpio100"),
	PINCTRL_PIN(101, "gpio101"),
	PINCTRL_PIN(102, "gpio102"),
	PINCTRL_PIN(103, "gpio103"),
	PINCTRL_PIN(104, "gpio104"),
	PINCTRL_PIN(105, "gpio105"),
	PINCTRL_PIN(106, "gpio106"),
	PINCTRL_PIN(107, "gpio107"),
	PINCTRL_PIN(108, "gpio108"),
	PINCTRL_PIN(109, "gpio109"),
	PINCTRL_PIN(110, "gpio110"),
	PINCTRL_PIN(111, "gpio111"),
	PINCTRL_PIN(112, "gpio112"),
	PINCTRL_PIN(113, "gpio113"),
	PINCTRL_PIN(114, "gpio114"),
	PINCTRL_PIN(115, "gpio115"),
	PINCTRL_PIN(116, "gpio116"),
	PINCTRL_PIN(117, "gpio117"),
	PINCTRL_PIN(118, "gpio118"),
	PINCTRL_PIN(119, "gpio119"),
	PINCTRL_PIN(120, "gpio120"),
	PINCTRL_PIN(121, "gpio121"),
	PINCTRL_PIN(122, "gpio122"),
	PINCTRL_PIN(123, "gpio123"),
	PINCTRL_PIN(124, "gpio124"),
	PINCTRL_PIN(125, "gpio125"),
	PINCTRL_PIN(126, "gpio126"),
	PINCTRL_PIN(127, "gpio127"),
	PINCTRL_PIN(128, "gpio128"),
	PINCTRL_PIN(129, "gpio129"),
	PINCTRL_PIN(130, "gpio130"),
	PINCTRL_PIN(131, "gpio131"),
	PINCTRL_PIN(132, "gpio132"),
	PINCTRL_PIN(133, "gpio133"),
	PINCTRL_PIN(134, "gpio134"),
	PINCTRL_PIN(135, "gpio135"),
	PINCTRL_PIN(136, "gpio136"),
	PINCTRL_PIN(137, "gpio137"),
	PINCTRL_PIN(138, "gpio138"),
	PINCTRL_PIN(139, "gpio139"),
	PINCTRL_PIN(140, "gpio140"),
	PINCTRL_PIN(141, "ufs_reset"),
	PINCTRL_PIN(142, "sdc1_rclk"),
	PINCTRL_PIN(143, "sdc1_clk"),
	PINCTRL_PIN(144, "sdc1_cmd"),
	PINCTRL_PIN(145, "sdc1_data"),
	PINCTRL_PIN(146, "sdc2_clk"),
	PINCTRL_PIN(147, "sdc2_cmd"),
	PINCTRL_PIN(148, "sdc2_data"),
};

static const int gpio0_pins[] = { 0 };
static const int gpio1_pins[] = { 1 };
static const int gpio2_pins[] = { 2 };
static const int gpio3_pins[] = { 3 };
static const int gpio4_pins[] = { 4 };
static const int gpio5_pins[] = { 5 };
static const int gpio6_pins[] = { 6 };
static const int gpio7_pins[] = { 7 };
static const int gpio8_pins[] = { 8 };
static const int gpio9_pins[] = { 9 };
static const int gpio10_pins[] = { 10 };
static const int gpio11_pins[] = { 11 };
static const int gpio12_pins[] = { 12 };
static const int gpio13_pins[] = { 13 };
static const int gpio14_pins[] = { 14 };
static const int gpio15_pins[] = { 15 };
static const int gpio16_pins[] = { 16 };
static const int gpio17_pins[] = { 17 };
static const int gpio18_pins[] = { 18 };
static const int gpio19_pins[] = { 19 };
static const int gpio20_pins[] = { 20 };
static const int gpio21_pins[] = { 21 };
static const int gpio22_pins[] = { 22 };
static const int gpio23_pins[] = { 23 };
static const int gpio24_pins[] = { 24 };
static const int gpio25_pins[] = { 25 };
static const int gpio26_pins[] = { 26 };
static const int gpio27_pins[] = { 27 };
static const int gpio28_pins[] = { 28 };
static const int gpio29_pins[] = { 29 };
static const int gpio30_pins[] = { 30 };
static const int gpio31_pins[] = { 31 };
static const int gpio32_pins[] = { 32 };
static const int gpio33_pins[] = { 33 };
static const int gpio34_pins[] = { 34 };
static const int gpio35_pins[] = { 35 };
static const int gpio36_pins[] = { 36 };
static const int gpio37_pins[] = { 37 };
static const int gpio38_pins[] = { 38 };
static const int gpio39_pins[] = { 39 };
static const int gpio40_pins[] = { 40 };
static const int gpio41_pins[] = { 41 };
static const int gpio42_pins[] = { 42 };
static const int gpio43_pins[] = { 43 };
static const int gpio44_pins[] = { 44 };
static const int gpio45_pins[] = { 45 };
static const int gpio46_pins[] = { 46 };
static const int gpio47_pins[] = { 47 };
static const int gpio48_pins[] = { 48 };
static const int gpio49_pins[] = { 49 };
static const int gpio50_pins[] = { 50 };
static const int gpio51_pins[] = { 51 };
static const int gpio52_pins[] = { 52 };
static const int gpio53_pins[] = { 53 };
static const int gpio54_pins[] = { 54 };
static const int gpio55_pins[] = { 55 };
static const int gpio56_pins[] = { 56 };
static const int gpio57_pins[] = { 57 };
static const int gpio58_pins[] = { 58 };
static const int gpio59_pins[] = { 59 };
static const int gpio60_pins[] = { 60 };
static const int gpio61_pins[] = { 61 };
static const int gpio62_pins[] = { 62 };
static const int gpio63_pins[] = { 63 };
static const int gpio64_pins[] = { 64 };
static const int gpio65_pins[] = { 65 };
static const int gpio66_pins[] = { 66 };
static const int gpio67_pins[] = { 67 };
static const int gpio68_pins[] = { 68 };
static const int gpio69_pins[] = { 69 };
static const int gpio70_pins[] = { 70 };
static const int gpio71_pins[] = { 71 };
static const int gpio72_pins[] = { 72 };
static const int gpio73_pins[] = { 73 };
static const int gpio74_pins[] = { 74 };
static const int gpio75_pins[] = { 75 };
static const int gpio76_pins[] = { 76 };
static const int gpio77_pins[] = { 77 };
static const int gpio78_pins[] = { 78 };
static const int gpio79_pins[] = { 79 };
static const int gpio80_pins[] = { 80 };
static const int gpio81_pins[] = { 81 };
static const int gpio82_pins[] = { 82 };
static const int gpio83_pins[] = { 83 };
static const int gpio84_pins[] = { 84 };
static const int gpio85_pins[] = { 85 };
static const int gpio86_pins[] = { 86 };
static const int gpio87_pins[] = { 87 };
static const int gpio88_pins[] = { 88 };
static const int gpio89_pins[] = { 89 };
static const int gpio90_pins[] = { 90 };
static const int gpio91_pins[] = { 91 };
static const int gpio92_pins[] = { 92 };
static const int gpio93_pins[] = { 93 };
static const int gpio94_pins[] = { 94 };
static const int gpio95_pins[] = { 95 };
static const int gpio96_pins[] = { 96 };
static const int gpio97_pins[] = { 97 };
static const int gpio98_pins[] = { 98 };
static const int gpio99_pins[] = { 99 };
static const int gpio100_pins[] = { 100 };
static const int gpio101_pins[] = { 101 };
static const int gpio102_pins[] = { 102 };
static const int gpio103_pins[] = { 103 };
static const int gpio104_pins[] = { 104 };
static const int gpio105_pins[] = { 105 };
static const int gpio106_pins[] = { 106 };
static const int gpio107_pins[] = { 107 };
static const int gpio108_pins[] = { 108 };
static const int gpio109_pins[] = { 109 };
static const int gpio110_pins[] = { 110 };
static const int gpio111_pins[] = { 111 };
static const int gpio112_pins[] = { 112 };
static const int gpio113_pins[] = { 113 };
static const int gpio114_pins[] = { 114 };
static const int gpio115_pins[] = { 115 };
static const int gpio116_pins[] = { 116 };
static const int gpio117_pins[] = { 117 };
static const int gpio118_pins[] = { 118 };
static const int gpio119_pins[] = { 119 };
static const int gpio120_pins[] = { 120 };
static const int gpio121_pins[] = { 121 };
static const int gpio122_pins[] = { 122 };
static const int gpio123_pins[] = { 123 };
static const int gpio124_pins[] = { 124 };
static const int gpio125_pins[] = { 125 };
static const int gpio126_pins[] = { 126 };
static const int gpio127_pins[] = { 127 };
static const int gpio128_pins[] = { 128 };
static const int gpio129_pins[] = { 129 };
static const int gpio130_pins[] = { 130 };
static const int gpio131_pins[] = { 131 };
static const int gpio132_pins[] = { 132 };
static const int gpio133_pins[] = { 133 };
static const int gpio134_pins[] = { 134 };
static const int gpio135_pins[] = { 135 };
static const int gpio136_pins[] = { 136 };
static const int gpio137_pins[] = { 137 };
static const int gpio138_pins[] = { 138 };
static const int gpio139_pins[] = { 139 };
static const int gpio140_pins[] = { 140 };
static const int ufs_reset_pins[] = { 141 };
static const int sdc1_rclk_pins[] = { 142 };
static const int sdc1_clk_pins[] = { 143 };
static const int sdc1_cmd_pins[] = { 144 };
static const int sdc1_data_pins[] = { 145 };
static const int sdc2_clk_pins[] = { 146 };
static const int sdc2_cmd_pins[] = { 147 };
static const int sdc2_data_pins[] = { 148 };

static const struct msm_pingroup sm7435_groups[] = {
	PINGROUP(0),
	PINGROUP(1),
	PINGROUP(2),
	PINGROUP(3),
	PINGROUP(4),
	PINGROUP(5),
	PINGROUP(6),
	PINGROUP(7),
	PINGROUP(8),
	PINGROUP(9),
	PINGROUP(10),
	PINGROUP(11),
	PINGROUP(12),
	PINGROUP(13),
	PINGROUP(14),
	PINGROUP(15),
	PINGROUP(16),
	PINGROUP(17),
	PINGROUP(18),
	PINGROUP(19),
	PINGROUP(20),
	PINGROUP(21),
	PINGROUP(22),
	PINGROUP(23),
	PINGROUP(24),
	PINGROUP(25),
	PINGROUP(26),
	PINGROUP(27),
	PINGROUP(28),
	PINGROUP(29),
	PINGROUP(30),
	PINGROUP(31),
	PINGROUP(32),
	PINGROUP(33),
	PINGROUP(34),
	PINGROUP(35),
	PINGROUP(36),
	PINGROUP(37),
	PINGROUP(38),
	PINGROUP(39),
	PINGROUP(40),
	PINGROUP(41),
	PINGROUP(42),
	PINGROUP(43),
	PINGROUP(44),
	PINGROUP(45),
	PINGROUP(46),
	PINGROUP(47),
	PINGROUP(48),
	PINGROUP(49),
	PINGROUP(50),
	PINGROUP(51),
	PINGROUP(52),
	PINGROUP(53),
	PINGROUP(54),
	PINGROUP(55),
	PINGROUP(56),
	PINGROUP(57),
	PINGROUP(58),
	PINGROUP(59),
	PINGROUP(60),
	PINGROUP(61),
	PINGROUP(62),
	PINGROUP(63),
	PINGROUP(64),
	PINGROUP(65),
	PINGROUP(66),
	PINGROUP(67),
	PINGROUP(68),
	PINGROUP(69),
	PINGROUP(70),
	PINGROUP(71),
	PINGROUP(72),
	PINGROUP(73),
	PINGROUP(74),
	PINGROUP(75),
	PINGROUP(76),
	PINGROUP(77),
	PINGROUP(78),
	PINGROUP(79),
	PINGROUP(80),
	PINGROUP(81),
	PINGROUP(82),
	PINGROUP(83),
	PINGROUP(84),
	PINGROUP(85),
	PINGROUP(86),
	PINGROUP(87),
	PINGROUP(88),
	PINGROUP(89),
	PINGROUP(90),
	PINGROUP(91),
	PINGROUP(92),
	PINGROUP(93),
	PINGROUP(94),
	PINGROUP(95),
	PINGROUP(96),
	PINGROUP(97),
	PINGROUP(98),
	PINGROUP(99),
	PINGROUP(100),
	PINGROUP(101),
	PINGROUP(102),
	PINGROUP(103),
	PINGROUP(104),
	PINGROUP(105),
	PINGROUP(106),
	PINGROUP(107),
	PINGROUP(108),
	PINGROUP(109),
	PINGROUP(110),
	PINGROUP(111),
	PINGROUP(112),
	PINGROUP(113),
	PINGROUP(114),
	PINGROUP(115),
	PINGROUP(116),
	PINGROUP(117),
	PINGROUP(118),
	PINGROUP(119),
	PINGROUP(120),
	PINGROUP(121),
	PINGROUP(122),
	PINGROUP(123),
	PINGROUP(124),
	PINGROUP(125),
	PINGROUP(126),
	PINGROUP(127),
	PINGROUP(128),
	PINGROUP(129),
	PINGROUP(130),
	PINGROUP(131),
	PINGROUP(132),
	PINGROUP(133),
	PINGROUP(134),
	PINGROUP(135),
	PINGROUP(136),
	PINGROUP(137),
	PINGROUP(138),
	PINGROUP(139),
	PINGROUP(140),
	[141] = UFS_RESET(ufs_reset, 0x19c000),
	[142] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x191004, 0, 0),
	[143] = SDC_QDSD_PINGROUP(sdc1_clk, 0x191000, 13, 6),
	[144] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x191000, 11, 3),
	[145] = SDC_QDSD_PINGROUP(sdc1_data, 0x191000, 9, 0),
	[146] = SDC_QDSD_PINGROUP(sdc2_clk, 0x194000, 14, 6),
	[147] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x194000, 11, 3),
	[148] = SDC_QDSD_PINGROUP(sdc2_data, 0x194000, 9, 0),
};

static const struct msm_gpio_wakeirq_map sm7435_pdc_map[] = {
	{ 0, 67 }, { 3, 82 }, { 4, 69 }, { 5, 70 }, { 6, 44 }, { 7, 43 },
	{ 8, 71 }, { 9, 86 }, { 10, 48 }, { 11, 77 }, { 12, 90 }, { 13, 54 },
	{ 14, 91 }, { 17, 97 }, { 18, 102 }, { 21, 103 }, { 22, 104 },
	{ 23, 105 }, { 24, 53 }, { 25, 106 }, { 26, 65 }, { 27, 55 }, { 28, 89 },
	{ 31, 109 }, { 32, 110 }, { 35, 59 }, { 37, 98 }, { 38, 111 }, { 43, 75 },
	{ 44, 114 }, { 45, 115 }, { 46, 116 }, { 47, 118 }, { 48, 95 },
	{ 52, 121 }, { 54, 94 }, { 55, 99 }, { 57, 52 }, { 59, 100 }, { 60, 113 },
	{ 61, 45 }, { 62, 85 }, { 63, 56 }, { 64, 84 }, { 65, 83 }, { 66, 96 },
	{ 67, 93 }, { 71, 122 }, { 72, 123 }, { 75, 42 }, { 76, 124 },
	{ 78, 125 }, { 80, 126 }, { 82, 127 }, { 84, 128 }, { 86, 129 },
	{ 88, 64 }, { 89, 130 }, { 91, 63 }, { 92, 101 }, { 95, 57 }, { 98, 66 },
	{ 99, 133 }, { 100, 76 }, { 101, 62 }, { 102, 134 }, { 103, 49 },
	{ 104, 73 }, { 105, 92 }, { 107, 46 }, { 109, 135 }, { 111, 136 },
	{ 112, 137 }, { 113, 58 }, { 115, 107 }, { 118, 108 }, { 125, 112 },
	{ 128, 72 }, { 130, 117 }, { 133, 119 }, { 135, 120 }, { 138, 131 },
	{ 140, 132 },
};

static const struct msm_pinctrl_soc_data sm7435_tlmm = {
	.pins = sm7435_pins,
	.npins = ARRAY_SIZE(sm7435_pins),
	.groups = sm7435_groups,
	.ngroups = ARRAY_SIZE(sm7435_groups),
	.ngpios = 141,
	.wakeirq_map = sm7435_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(sm7435_pdc_map),
};

static int sm7435_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &sm7435_tlmm);
}

static const struct of_device_id sm7435_pinctrl_of_match[] = {
	{ .compatible = "qcom,sm7435-tlmm", .data = &sm7435_tlmm },
	{ }
};
MODULE_DEVICE_TABLE(of, sm7435_pinctrl_of_match);

static struct platform_driver sm7435_pinctrl_driver = {
	.driver = {
		.name = "sm7435-pinctrl",
		.of_match_table = sm7435_pinctrl_of_match,
	},
	.probe = sm7435_pinctrl_probe,
};

static int __init sm7435_pinctrl_init(void)
{
	return platform_driver_register(&sm7435_pinctrl_driver);
}
arch_initcall(sm7435_pinctrl_init);

static void __exit sm7435_pinctrl_exit(void)
{
	platform_driver_unregister(&sm7435_pinctrl_driver);
}
module_exit(sm7435_pinctrl_exit);

MODULE_DESCRIPTION("Qualcomm SM7435 TLMM pin controller (GPIO only)");
MODULE_LICENSE("GPL");
