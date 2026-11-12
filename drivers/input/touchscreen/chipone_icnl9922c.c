// SPDX-License-Identifier: GPL-2.0-only
/* ICNL9922C TCS SPI input bring-up. Protocol reference: Motorola portov v3. */
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/spi/spi.h>
#include <linux/unaligned.h>

#define ICNL_CONTACTS 10
#define ICNL_INFO_LEN 112
#define ICNL_PACKET_LEN (ICNL_INFO_LEN + 5)
#define ICNL_PROGRAM_ADDR 0x60
#define ICNL_PROGRAM_PAYLOAD_MAX 8189
#define ICNL_PROGRAM_XFER_MAX (ICNL_PROGRAM_PAYLOAD_MAX + 18)
#define ICNL_FIFO_XFER_MAX 32
#define ICNL_XFERS_MAX DIV_ROUND_UP(ICNL_PROGRAM_XFER_MAX, ICNL_FIFO_XFER_MAX)
#define ICNL_HW_BOOT_MODE 0x70010
#define ICNL_HW_CURRENT_MODE 0x70011
#define ICNL_BOOT_MODE_SRAM 3
#define ICNL_PROGRAM_MODE 5
#define ICNL_PROGRAM_MODE_ALT 2
#define ICNL_BOOT_MAGIC_ADDR 0x02bff0
#define ICNL_BOOT_CRC_ADDR 0x02bff8
#define ICNL_BOOT_SIZE_ADDR 0x02bffc
#define ICNL_CRC_SRAM_ADDR 0x74008
#define ICNL_CRC_LENGTH 0x7400c
#define ICNL_CRC_RESULT 0x7401c
#define ICNL_CRC_START 0x74020
#define ICNL_CRC_BUSY 0x74024
#define ICNL_FIRMWARE_MAX (96 * 1024)

/* May be disabled on the command line for staged bring-up. */
static bool probe_enabled = true;
static bool reset_on_probe;
module_param(reset_on_probe, bool, 0644);
MODULE_PARM_DESC(reset_on_probe, "Pulse reset instead of preserving running firmware");
module_param(probe_enabled, bool, 0644);
MODULE_PARM_DESC(probe_enabled, "Enable probe (disable for staged bring-up)");

struct icnl_ts {
	struct spi_device *spi;
	struct input_dev *input;
	struct gpio_desc *reset;
	const char *firmware_name;
	struct touchscreen_properties prop;
	u8 tx[ICNL_PACKET_LEN];
	u8 rx[ICNL_PACKET_LEN];
	u8 *program_tx;
	u8 *program_rx;
	struct spi_transfer *xfers;
	atomic64_t frames;
	atomic64_t errors;
};

static u16 icnl_crc(const u8 *data, size_t len)
{
	u16 crc = 0;
	int i;

	while (len--) {
		crc ^= (u16)*data++ << 8;
		for (i = 0; i < 8; i++)
			crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
	}
	return crc;
}

static u32 icnl_crc32(const u8 *data, size_t len)
{
	u32 crc = 0;
	int i;

	while (len--) {
		crc ^= (u32)*data++ << 24;
		for (i = 0; i < 8; i++)
			crc = (crc << 1) ^ ((crc & 0x80000000) ? 0x04c11db7 : 0);
	}
	return crc;
}

static void icnl_header(struct icnl_ts *ts, u16 cmd, u16 len)
{
	memset(ts->tx, 0, sizeof(ts->tx));
	memset(ts->rx, 0, sizeof(ts->rx));
	ts->tx[0] = 0xf1;
	put_unaligned_le16(cmd, ts->tx + 1);
	put_unaligned_le16(len, ts->tx + 3);
	put_unaligned_le16(icnl_crc(ts->tx, 5), ts->tx + 5);
}

static int icnl_check(struct icnl_ts *ts, size_t len, u16 cmd)
{
	if (get_unaligned_le16(ts->rx + len - 2) != icnl_crc(ts->rx, len - 2))
		return -EBADMSG;
	if (ts->rx[len - 5] || get_unaligned_le16(ts->rx + len - 4) != cmd)
		return -EPROTO;
	return 0;
}

/* Keep CS asserted across FIFO transfers; SE DMA has not been brought up. */
static int icnl_transfer(struct icnl_ts *ts, const u8 *tx, u8 *rx, size_t len)
{
	size_t offset = 0;
	unsigned int n = 0;

	if (!len || len > ICNL_PROGRAM_XFER_MAX)
		return -EINVAL;
	memset(ts->xfers, 0, sizeof(*ts->xfers) *
	       DIV_ROUND_UP(len, ICNL_FIFO_XFER_MAX));
	while (offset < len) {
		size_t chunk = min_t(size_t, len - offset, ICNL_FIFO_XFER_MAX);

		ts->xfers[n].tx_buf = tx + offset;
		ts->xfers[n].rx_buf = rx + offset;
		ts->xfers[n].len = chunk;
		offset += chunk;
		n++;
	}
	return spi_sync_transfer(ts->spi, ts->xfers, n);
}

static int icnl_program_xfer(struct icnl_ts *ts, bool read, u32 addr,
			     void *data, size_t len)
{
	size_t total = 3 + len + 15;
	int ret;

	if (len > ICNL_PROGRAM_PAYLOAD_MAX || total > ICNL_PROGRAM_XFER_MAX)
		return -EINVAL;

	memset(ts->program_tx, 0, total);
	memset(ts->program_rx, 0, total);
	ts->program_tx[0] = read ? 0x61 : ICNL_PROGRAM_ADDR;
	put_unaligned_be24(addr, ts->program_tx + 1);
	put_unaligned_be24(len, ts->program_tx + 4);
	put_unaligned_be16(~icnl_crc(ts->program_tx, 7),
			   ts->program_tx + 7);
	if (!read) {
		memcpy(ts->program_tx + 13, data, len);
		put_unaligned_be16(~icnl_crc(data, len),
				   ts->program_tx + 13 + len);
	}

	ret = icnl_transfer(ts, ts->program_tx, ts->program_rx, total);
	usleep_range(100, 150);
	if (ret)
		return ret;
	if (read) {
		if (get_unaligned_be16(ts->program_rx + 13 + len) !=
		    (u16)~icnl_crc(ts->program_rx + 13, len))
			return -EBADMSG;
		memcpy(data, ts->program_rx + 13, len);
	}
	return 0;
}

static int icnl_program_write(struct icnl_ts *ts, u32 addr, const void *data,
			      size_t len)
{
	return icnl_program_xfer(ts, false, addr, (void *)data, len);
}

static int icnl_program_read(struct icnl_ts *ts, u32 addr, void *data, size_t len)
{
	return icnl_program_xfer(ts, true, addr, data, len);
}

static int icnl_program_write32(struct icnl_ts *ts, u32 addr, u32 value)
{
	__le32 word = cpu_to_le32(value);

	return icnl_program_write(ts, addr, &word, sizeof(word));
}

static int icnl_verify_sram(struct icnl_ts *ts, u32 size, u32 expected)
{
	__le32 result;
	u8 status = 1;
	int ret, i;

	ret = icnl_program_write32(ts, ICNL_CRC_SRAM_ADDR, 0);
	if (ret)
		return ret;
	usleep_range(1000, 1500);
	ret = icnl_program_write32(ts, ICNL_CRC_LENGTH, size);
	if (ret)
		return ret;
	usleep_range(1000, 1500);
	ret = icnl_program_write(ts, ICNL_CRC_START, &status, 1);
	if (ret)
		return ret;
	msleep(10);
	for (i = 0; i < 50; i++) {
		ret = icnl_program_read(ts, ICNL_CRC_BUSY, &status, 1);
		if (ret)
			return ret;
		if (!status)
			break;
		msleep(20);
	}
	if (status)
		return -ETIMEDOUT;
	ret = icnl_program_read(ts, ICNL_CRC_RESULT, &result, sizeof(result));
	if (ret)
		return ret;
	if (le32_to_cpu(result) != expected) {
		dev_err(&ts->spi->dev, "SRAM CRC %08x, expected %08x\n",
			le32_to_cpu(result), expected);
		return -EBADMSG;
	}
	return 0;
}

static int icnl_load_firmware(struct icnl_ts *ts)
{
	const struct firmware *fw;
	const u8 *p;
	u8 magic[] = { 0xcc, 0x33, 0x55, 0x5a };
	u8 boot_mode;
	__le32 hwid;
	u32 crc;
	size_t left;
	int ret;

	ret = request_firmware(&fw, ts->firmware_name, &ts->spi->dev);
	if (ret)
		return dev_err_probe(&ts->spi->dev, ret,
				     "request touch firmware %s\n", ts->firmware_name);
	if (fw->size <= 0x102 || fw->size > ICNL_FIRMWARE_MAX) {
		ret = -EINVAL;
		goto out;
	}

	ret = spi_write(ts->spi, magic, sizeof(magic));
	if (ret)
		goto out;
	msleep(5);
	ret = icnl_program_read(ts, ICNL_HW_CURRENT_MODE, &boot_mode, 1);
	if (ret)
		goto out;
	if ((boot_mode & 7) != ICNL_PROGRAM_MODE &&
	    (boot_mode & 7) != ICNL_PROGRAM_MODE_ALT) {
		ret = -EPROTO;
		goto out;
	}
	ret = icnl_program_read(ts, 0x70000, &hwid, sizeof(hwid));
	if (ret)
		goto out;
	if ((le32_to_cpu(hwid) & 0xfffffff0) != 0x99c220) {
		dev_err(&ts->spi->dev, "unexpected hardware ID %08x\n",
			le32_to_cpu(hwid));
		ret = -ENODEV;
		goto out;
	}
	dev_info(&ts->spi->dev, "program mode=%u, uploading %zu bytes\n",
		 boot_mode & 7, fw->size);

	p = fw->data;
	left = fw->size;
	while (left) {
		size_t chunk = min_t(size_t, left, ICNL_PROGRAM_PAYLOAD_MAX);
		size_t offset = p - fw->data;

		ret = icnl_program_write(ts, offset, p, chunk);
		if (ret)
			goto out;
		p += chunk;
		left -= chunk;
	}

	crc = icnl_crc32(fw->data, fw->size);
	dev_info(&ts->spi->dev, "SRAM upload complete, verifying CRC=%08x\n", crc);
	ret = icnl_verify_sram(ts, fw->size, crc);
	if (ret)
		goto out;
	ret = icnl_program_write32(ts, ICNL_BOOT_MAGIC_ADDR, 0xcc33555a);
	if (ret)
		goto out;
	ret = icnl_program_write32(ts, ICNL_BOOT_CRC_ADDR, crc);
	if (ret)
		goto out;
	ret = icnl_program_write32(ts, ICNL_BOOT_SIZE_ADDR, fw->size);
	if (ret)
		goto out;
	boot_mode = ICNL_BOOT_MODE_SRAM;
	ret = icnl_program_write(ts, ICNL_HW_BOOT_MODE, &boot_mode, 1);
	if (ret)
		goto out;
	msleep(30);
	dev_info(&ts->spi->dev, "loaded firmware size=%zu crc=%08x\n",
		 fw->size, crc);

out:
	release_firmware(fw);
	return ret;
}

static int icnl_read(struct icnl_ts *ts, u16 cmd, size_t len)
{
	int ret;

	if (len + 5 > sizeof(ts->rx))
		return -EINVAL;
	icnl_header(ts, cmd, len);
	ret = spi_write(ts->spi, ts->tx, 7);
	if (ret)
		return ret;
	usleep_range(100, 150);
	ret = spi_read(ts->spi, ts->rx, len + 5);
	if (ret)
		return ret;
	ret = icnl_check(ts, len + 5, cmd);
	if (ret)
		dev_warn(&ts->spi->dev, "cmd=%04x tx=%7ph rx=%*ph status=%d\n",
			 cmd, ts->tx, (int)len + 5, ts->rx, ret);
	return ret;
}

static irqreturn_t icnl_irq(int irq, void *data)
{
	struct icnl_ts *ts = data;
	unsigned long active = 0;
	u8 count, id, event;
	u16 x, y;
	int ret, i;

	icnl_header(ts, 0x4103, ICNL_PACKET_LEN);
	ret = icnl_transfer(ts, ts->tx, ts->rx, ICNL_PACKET_LEN);
	if (!ret)
		ret = icnl_check(ts, ICNL_PACKET_LEN, 0x4103);
	if (ret)
		goto bad;
	count = ts->rx[1];
	if (count > ICNL_CONTACTS)
		goto bad;
	/* Validate the whole frame before emitting any partial input state. */
	for (i = 0; i < count; i++) {
		const u8 *p = ts->rx + 2 + i * 7;
		if (p[0] >= ICNL_CONTACTS || p[6] > 4)
			goto bad;
	}
	for (i = 0; i < count; i++) {
		const u8 *p = ts->rx + 2 + i * 7;

		id = p[0];
		event = p[6];
		if (!event)
			continue;
		input_mt_slot(ts->input, id);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, event != 4);
		if (event == 4)
			continue;
		x = get_unaligned_le16(p + 1);
		y = get_unaligned_le16(p + 3);
		touchscreen_report_pos(ts->input, &ts->prop, x, y, true);
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, p[5]);
		active |= BIT(id);
	}
	input_mt_sync_frame(ts->input);
	input_report_key(ts->input, BTN_TOUCH, !!active);
	input_sync(ts->input);
	atomic64_inc(&ts->frames);
	return IRQ_HANDLED;

bad:
	atomic64_inc(&ts->errors);
	dev_warn_ratelimited(&ts->spi->dev, "invalid touch packet (%d)\n", ret);
	return IRQ_HANDLED;
}

static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct icnl_ts *ts = dev_get_drvdata(dev);

	return sysfs_emit(buf, "frames=%lld errors=%lld\n",
			 atomic64_read(&ts->frames), atomic64_read(&ts->errors));
}
static DEVICE_ATTR_RO(stats);
static struct attribute *icnl_attrs[] = { &dev_attr_stats.attr, NULL };
static const struct attribute_group icnl_group = { .attrs = icnl_attrs };

static int icnl_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct icnl_ts *ts;
	u16 fwid, version, max_x, max_y;
	int ret;

	if (!probe_enabled)
		return -ENODEV;
	if (spi->irq <= 0)
		return dev_err_probe(dev, -EINVAL, "missing interrupt\n");
	ts = devm_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	ts->program_tx = devm_kzalloc(dev, ICNL_PROGRAM_XFER_MAX, GFP_KERNEL);
	ts->program_rx = devm_kzalloc(dev, ICNL_PROGRAM_XFER_MAX, GFP_KERNEL);
	ts->xfers = devm_kcalloc(dev, ICNL_XFERS_MAX, sizeof(*ts->xfers), GFP_KERNEL);
	if (!ts->program_tx || !ts->program_rx || !ts->xfers)
		return -ENOMEM;
	ts->spi = spi;
	ret = device_property_read_string(dev, "firmware-name", &ts->firmware_name);
	if (ret)
		return dev_err_probe(dev, ret, "missing firmware-name\n");
	spi_set_drvdata(spi, ts);
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		return ret;
	ts->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ts->reset))
		return dev_err_probe(dev, PTR_ERR(ts->reset), "reset GPIO\n");
	if (reset_on_probe) {
		/* Vendor reset timing; opt-in because firmware may live in SRAM. */
		usleep_range(1000, 1500);
		gpiod_set_value_cansleep(ts->reset, 1);
		msleep(10);
		gpiod_set_value_cansleep(ts->reset, 0);
		msleep(100);
	}
	ret = icnl_read(ts, 0x4003, 4);
	if (ret) {
		dev_info(dev, "running firmware unavailable; loading SRAM image\n");
		ret = icnl_load_firmware(ts);
		if (ret)
			return dev_err_probe(dev, ret, "load firmware\n");
		ret = icnl_read(ts, 0x4003, 4);
		if (ret)
			return dev_err_probe(dev, ret, "firmware ID read\n");
	}
	fwid = get_unaligned_le16(ts->rx);
	if (fwid != 0x9965)
		return dev_err_probe(dev, -ENODEV, "unexpected firmware ID %04x\n", fwid);
	ret = icnl_read(ts, 0x4005, 4);
	if (ret)
		return ret;
	version = get_unaligned_le16(ts->rx);
	ret = icnl_read(ts, 0x4007, 10);
	if (ret)
		return ret;
	max_x = get_unaligned_le16(ts->rx);
	max_y = get_unaligned_le16(ts->rx + 2);
	if (!max_x || !max_y)
		return -EINVAL;
	ts->input = devm_input_allocate_device(dev);
	if (!ts->input)
		return -ENOMEM;
	ts->input->name = "Chipone ICNL9922C";
	ts->input->id.bustype = BUS_SPI;
	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_capability(ts->input, EV_KEY, BTN_TOUCH);
	touchscreen_parse_properties(ts->input, true, &ts->prop);
	ret = input_mt_init_slots(ts->input, ICNL_CONTACTS, INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (ret)
		return ret;
	ret = input_register_device(ts->input);
	if (ret)
		return ret;
	ret = devm_request_threaded_irq(dev, spi->irq, NULL, icnl_irq, IRQF_ONESHOT,
				      dev_name(dev), ts);
	if (ret)
		return ret;
	ret = devm_device_add_group(dev, &icnl_group);
	if (ret)
		return ret;
	dev_info(dev, "FWID=%04x firmware=%04x coordinates=%ux%u\n",
		 fwid, version, max_x, max_y);
	return 0;
}

static const struct of_device_id icnl_of_match[] = {
	{ .compatible = "chipone,icnl9922c" },
	{ }
};
MODULE_DEVICE_TABLE(of, icnl_of_match);

static const struct spi_device_id icnl_ids[] = {
	{ "icnl9922c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, icnl_ids);

static struct spi_driver icnl_driver = {
	.driver = {
		.name = "chipone-icnl9922c",
		.of_match_table = icnl_of_match,
	},
	.probe = icnl_probe,
	.id_table = icnl_ids,
};
module_spi_driver(icnl_driver);
MODULE_DESCRIPTION("Chipone ICNL9922C TCS SPI touchscreen");
MODULE_LICENSE("GPL");
