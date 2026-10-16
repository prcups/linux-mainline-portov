// SPDX-License-Identifier: GPL-2.0-only
/*
 * portov diagnostic on-screen console.
 *
 * Every other observability channel failed: the PSCI soft reset is
 * ineffective under the Gunyah/ABL handoff, the mainline ramoops record is
 * corrupted by the hard-reset recovery, and CDC ACM never enumerates.  An
 * entry-point paint proved both that the bootloader hands control to the
 * mainline Image and that the panel scans the continuous-splash framebuffer
 * at 0xb8000000.
 *
 * simplefb/fbcon never put anything on that buffer, and neither does this
 * driver if it maps it the obvious way: on arm64 both ioremap() and
 * memremap(MEMREMAP_WC) refuse or fail for memory that is part of the linear
 * map, and the splash region is reserved-memory without "no-map", i.e. plain
 * System RAM.  The region is already mapped, so use the linear map and clean
 * the touched cache lines - the display engine reads DRAM directly.
 *
 * With CON_PRINTBUFFER the whole accumulated boot log is replayed the moment
 * the console registers, so nothing that happened earlier is lost.
 *
 * Diagnostic only; not intended for upstream.
 */

#include <asm/cacheflush.h>
#include <linux/console.h>
#include <linux/font.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>

#define PORTOV_FB_PHYS		0xb8000000
#define PORTOV_FB_WIDTH		1080
#define PORTOV_FB_HEIGHT	2400
#define PORTOV_FB_STRIDE	(PORTOV_FB_WIDTH * 4)

#define PORTOV_FB_FG		0x00ffffff
#define PORTOV_FB_BG		0x00000000

static u32 *portov_fb;
static const struct font_desc *portov_font;
static unsigned int portov_cols;
static unsigned int portov_rows;
static unsigned int portov_x;
static unsigned int portov_y;
static unsigned int portov_dirty_lo = ~0u;
static unsigned int portov_dirty_hi;

static void portov_mark_dirty(unsigned int row)
{
	if (row < portov_dirty_lo)
		portov_dirty_lo = row;
	if (row > portov_dirty_hi)
		portov_dirty_hi = row;
}

static void portov_flush_dirty(void)
{
	unsigned long start, end;

	if (portov_dirty_lo > portov_dirty_hi)
		return;

	start = (unsigned long)(portov_fb + portov_dirty_lo * PORTOV_FB_WIDTH);
	end = (unsigned long)(portov_fb +
			      (portov_dirty_hi + 1) * PORTOV_FB_WIDTH);
	dcache_clean_poc(start, end);

	portov_dirty_lo = ~0u;
	portov_dirty_hi = 0;
}

static void portov_fb_fill(u32 colour)
{
	unsigned int i;

	for (i = 0; i < PORTOV_FB_WIDTH * PORTOV_FB_HEIGHT; i++)
		portov_fb[i] = colour;

	dcache_clean_poc((unsigned long)portov_fb,
			 (unsigned long)(portov_fb +
					 PORTOV_FB_WIDTH * PORTOV_FB_HEIGHT));
	portov_dirty_lo = ~0u;
	portov_dirty_hi = 0;
}

static void portov_fb_glyph(unsigned int col, unsigned int row, unsigned char ch)
{
	const unsigned char *glyph;
	unsigned int x, y, i, j;

	glyph = (const unsigned char *)portov_font->data +
		(unsigned int)ch * portov_font->height;
	x = col * portov_font->width;
	y = row * portov_font->height;

	for (j = 0; j < portov_font->height; j++) {
		u32 *line = portov_fb + (y + j) * PORTOV_FB_WIDTH + x;

		for (i = 0; i < portov_font->width; i++)
			line[i] = (glyph[j] & (0x80 >> i)) ?
				  PORTOV_FB_FG : PORTOV_FB_BG;
	}

	portov_mark_dirty(y);
	portov_mark_dirty(y + portov_font->height - 1);
}

static void portov_newline(void)
{
	portov_x = 0;
	portov_y++;

	if (portov_y >= portov_rows) {
		/* Keep the newest lines: restart from a clean screen. */
		portov_fb_fill(PORTOV_FB_BG);
		portov_y = 0;
	}
}

static void portov_console_write(struct console *co, const char *s,
				 unsigned int count)
{
	static const char * const keep[] = {
		"portov", "dwc3", "usb", "USB", "phy", "PHY", "scmi", "SCMI",
		"clk", "Clk", "gpio", "GPIO", "defer", "error", "Error",
		"fail", "Fail", "abort", "BUG", "WARNING", "Call trace",
		"initramfs", "udc", "UDC", "regulator", "reset",
		"endpoint", "gadget", "descriptor", "composite", "setup",
		"ssusb", "dwc", "hsphy", "xhci",
	};
	/*
	 * Noise that repeats without bound and would scroll the useful lines off
	 * the fixed-size panel: the gadget event path logs one of these per
	 * malformed event, and a failing enumeration produces thousands.  The
	 * message itself is already understood; keep it off the console so the
	 * one-shot boot messages stay readable.
	 */
	static const char * const drop[] = {
		"unknown endpoint event",
	};
	static char line[200];
	static unsigned int line_len;
	const char *hit;
	unsigned int i;

	if (!portov_fb || !portov_font)
		return;

	while (count--) {
		unsigned char ch = *s++;

		if (ch == '\n') {
			line[line_len] = '\0';
			line_len = 0;

			/*
			 * Show only the lines that matter for this bring-up: the
			 * full boot log scrolls far too fast to read off a 150-line
			 * panel and the interesting lines are a small minority.
			 */
			for (i = 0; i < ARRAY_SIZE(drop); i++)
				if (strstr(line, drop[i]))
					break;
			if (i < ARRAY_SIZE(drop))
				continue;

			hit = NULL;
			for (i = 0; i < ARRAY_SIZE(keep) && !hit; i++)
				if (strstr(line, keep[i]))
					hit = keep[i];
			if (!line[0] || !hit)
				continue;

			for (i = 0; line[i]; i++) {
				if (portov_x >= portov_cols)
					portov_newline();
				portov_fb_glyph(portov_x, portov_y, line[i]);
				portov_x++;
			}
			portov_newline();
			continue;
		}
		if (ch == '\r')
			continue;
		if (line_len < sizeof(line) - 1)
			line[line_len++] = ch;
	}

	portov_flush_dirty();
}

static struct console portov_console = {
	.name	= "portovfb",
	.write	= portov_console_write,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
};

static int __init portov_diag_console(void)
{
	/*
	 * The splash region is reserved-memory without "no-map", so it is
	 * already covered by the linear map; ioremap() would be refused and
	 * memremap(MEMREMAP_WC) has no mapping to hand out for System RAM.
	 */
	portov_fb = (u32 *)phys_to_virt(PORTOV_FB_PHYS);

	portov_font = find_font("VGA8x16");
	if (!portov_font) {
		unsigned long font_w = 8, font_h = 16;

		portov_font = get_default_font(PORTOV_FB_WIDTH, PORTOV_FB_HEIGHT,
					       &font_w, &font_h);
	}
	if (!portov_font) {
		pr_warn("portov-diag-console: no usable font\n");
		return 0;
	}

	portov_cols = PORTOV_FB_WIDTH / portov_font->width;
	portov_rows = PORTOV_FB_HEIGHT / portov_font->height;

	portov_fb_fill(PORTOV_FB_BG);

	register_console(&portov_console);

	pr_info("portov-diag-console: %ux%u text console on the panel\n",
		portov_cols, portov_rows);

	return 0;
}

/*
 * Late enough that the font and the linear map are ready, early enough that
 * the rest of the boot is visible live; CON_PRINTBUFFER replays everything
 * before it.
 */
device_initcall(portov_diag_console);
