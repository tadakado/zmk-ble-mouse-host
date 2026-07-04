// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tadashi Kadowaki
#include <zmk_ble_mouse_host/hid_parser.h>

/* Short-item tags (prefix byte masked with 0xFC), per the HID 1.11 spec. */
#define ITEM_USAGE_PAGE     0x04
#define ITEM_USAGE          0x08
#define ITEM_USAGE_MINIMUM  0x18
#define ITEM_USAGE_MAXIMUM  0x28
#define ITEM_REPORT_SIZE    0x74
#define ITEM_REPORT_ID      0x84
#define ITEM_REPORT_COUNT   0x94
#define ITEM_INPUT          0x80
#define ITEM_OUTPUT         0x90
#define ITEM_FEATURE        0xB0
#define ITEM_COLLECTION     0xA0
#define ITEM_COLLECTION_END 0xC0

/* Usage pages */
#define PAGE_GENERIC        0x01
#define PAGE_BUTTON         0x09
#define PAGE_CONSUMER       0x0C

/* Generic Desktop usages */
#define USAGE_X             0x30
#define USAGE_Y             0x31
#define USAGE_WHEEL         0x38
/* Consumer usage: AC Pan = horizontal wheel */
#define USAGE_AC_PAN        0x0238

#define MAX_LOCAL_USAGES    8

static void set_field(HidField *f, unsigned short bitOffset, unsigned char bitSize,
		      unsigned char reportId)
{
	if (f->valid)
		return; /* first declaration wins */
	f->valid = 1;
	f->bitOffset = bitOffset;
	f->bitSize = bitSize;
	f->reportId = reportId;
}

void hid_parse_mouse(const unsigned char *desc, unsigned short len, MouseLayout *out)
{
	unsigned short i;
	unsigned char usagePage = 0;
	unsigned long reportSize = 0;
	unsigned long reportCount = 0;
	unsigned char reportId = 0;
	unsigned char hasReportId = 0;
	unsigned short bitOffset = 0; /* position within the current report */

	unsigned long localUsage[MAX_LOCAL_USAGES];
	unsigned char localUsageCount = 0;
	unsigned long usageMin = 0;
	unsigned char hasUsageMin = 0;

	/* clear output */
	{
		unsigned char *p = (unsigned char *)out;
		for (i = 0; i < sizeof(MouseLayout); i++)
			p[i] = 0;
	}

	i = 0;
	while (i < len) {
		unsigned char id = desc[i] & 0xFC;
		unsigned char size = desc[i] & 0x03;
		unsigned long data = 0;
		unsigned char j;
		if (size == 3)
			size = 4;
		if ((unsigned short)(i + 1 + size) > len)
			break; /* truncated / malformed descriptor */
		for (j = 0; j < size; j++)
			data |= ((unsigned long)desc[i + 1 + j]) << (j * 8);

		switch (id) {
		case ITEM_USAGE_PAGE:
			usagePage = (unsigned char)data;
			break;
		case ITEM_REPORT_SIZE:
			reportSize = data;
			break;
		case ITEM_REPORT_COUNT:
			reportCount = data;
			break;
		case ITEM_REPORT_ID:
			reportId = (unsigned char)data;
			hasReportId = 1;
			bitOffset = 8; /* the 1-byte report id precedes the fields */
			break;
		case ITEM_USAGE:
			if (localUsageCount < MAX_LOCAL_USAGES)
				localUsage[localUsageCount++] = data;
			break;
		case ITEM_USAGE_MINIMUM:
			usageMin = data;
			hasUsageMin = 1;
			break;
		case ITEM_USAGE_MAXIMUM:
			break;
		case ITEM_COLLECTION:
		case ITEM_COLLECTION_END:
			localUsageCount = 0;
			hasUsageMin = 0;
			break;
		case ITEM_INPUT: {
			unsigned long span = reportSize * reportCount;
			/* data bit0=0 -> data field (not constant padding) */
			unsigned char isConst = (unsigned char)(data & 0x01);
			if (!isConst) {
				if (usagePage == PAGE_BUTTON) {
					set_field(&out->buttons, bitOffset,
						  (unsigned char)(span > 255 ? 255 : span),
						  reportId);
				} else {
					unsigned long f;
					for (f = 0; f < reportCount; f++) {
						unsigned long u;
						unsigned short fbit =
							(unsigned short)(bitOffset + f * reportSize);
						if (f < localUsageCount)
							u = localUsage[f];
						else if (hasUsageMin)
							u = usageMin + f;
						else if (localUsageCount)
							u = localUsage[localUsageCount - 1];
						else
							u = 0;
						if (usagePage == PAGE_GENERIC) {
							if (u == USAGE_X)
								set_field(&out->x, fbit,
									  (unsigned char)reportSize, reportId);
							else if (u == USAGE_Y)
								set_field(&out->y, fbit,
									  (unsigned char)reportSize, reportId);
							else if (u == USAGE_WHEEL)
								set_field(&out->wheel, fbit,
									  (unsigned char)reportSize, reportId);
						} else if (usagePage == PAGE_CONSUMER) {
							if (u == USAGE_AC_PAN)
								set_field(&out->hwheel, fbit,
									  (unsigned char)reportSize, reportId);
						}
					}
				}
			}
			bitOffset = (unsigned short)(bitOffset + span);
			localUsageCount = 0;
			hasUsageMin = 0;
			break;
		}
		case ITEM_OUTPUT:
		case ITEM_FEATURE:
			/* advance offset and reset locals like any main item */
			bitOffset = (unsigned short)(bitOffset + reportSize * reportCount);
			localUsageCount = 0;
			hasUsageMin = 0;
			break;
		default:
			break;
		}
		i += size + 1;
	}

	out->hasReportId = hasReportId;
	/* The mouse report is the one carrying X. Drop fields from other reports. */
	if (out->x.valid) {
		out->mouseReportId = out->x.reportId;
		if (out->y.valid && out->y.reportId != out->mouseReportId)
			out->y.valid = 0;
		if (out->buttons.valid && out->buttons.reportId != out->mouseReportId)
			out->buttons.valid = 0;
		if (out->wheel.valid && out->wheel.reportId != out->mouseReportId)
			out->wheel.valid = 0;
		if (out->hwheel.valid && out->hwheel.reportId != out->mouseReportId)
			out->hwheel.valid = 0;
	}
}

/* Read nbits (<=32) as unsigned, little-endian bit order matching HID. */
static unsigned long read_bits_u(const unsigned char *buf, unsigned short bitoff,
				 unsigned char nbits)
{
	unsigned long val = 0;
	unsigned char k;
	for (k = 0; k < nbits; k++) {
		unsigned short b = (unsigned short)(bitoff + k);
		unsigned char bit = (buf[b >> 3] >> (b & 7)) & 1;
		val |= ((unsigned long)bit) << k;
	}
	return val;
}

/* Read nbits as a sign-extended signed value (no clamping). */
static long read_signed(const unsigned char *buf, unsigned short bitoff, unsigned char nbits)
{
	long val;
	if (nbits == 0 || nbits > 32)
		return 0;
	val = (long)read_bits_u(buf, bitoff, nbits);
	if (nbits < 32 && (val & (1L << (nbits - 1))))
		val -= (1L << nbits); /* sign extend */
	return val;
}

static signed short clamp16(long v)
{
	if (v > 32767)
		return 32767;
	if (v < -32768)
		return -32768;
	return (signed short)v;
}

static signed char clamp8(long v)
{
	if (v > 127)
		return 127;
	if (v < -128)
		return -128;
	return (signed char)v;
}

unsigned char hid_extract_mouse(const MouseLayout *layout,
				const unsigned char *report, unsigned short len,
				MouseState *st)
{
	st->buttons = 0;
	st->dx = 0;
	st->dy = 0;
	st->wheel = 0;
	st->hwheel = 0;

	/* All field offsets are measured in bits within the report. A field is only
	 * decoded if it fully fits inside the actual report length; this guards
	 * against a device sending a shorter/different report than its descriptor
	 * implied (otherwise wheel/hwheel could pick up X/Y bytes). */
	unsigned short lenbits = (unsigned short)len << 3;

	if (!layout->x.valid)
		return 0;

	if (layout->mouseReportId) {
		if (len < 1 || report[0] != layout->mouseReportId)
			return 0;
	}

	/* X is mandatory; if it does not fit, this is not the expected report. */
	if ((unsigned short)(layout->x.bitOffset + layout->x.bitSize) > lenbits)
		return 0;

	if (layout->buttons.valid &&
	    (unsigned short)(layout->buttons.bitOffset + layout->buttons.bitSize) <= lenbits) {
		unsigned char nb = layout->buttons.bitSize;
		if (nb > 16)
			nb = 16; /* keep the first 16 buttons */
		st->buttons = (unsigned short)read_bits_u(report, layout->buttons.bitOffset, nb);
	}
	st->dx = clamp16(read_signed(report, layout->x.bitOffset, layout->x.bitSize));
	if (layout->y.valid &&
	    (unsigned short)(layout->y.bitOffset + layout->y.bitSize) <= lenbits)
		st->dy = clamp16(read_signed(report, layout->y.bitOffset, layout->y.bitSize));
	if (layout->wheel.valid &&
	    (unsigned short)(layout->wheel.bitOffset + layout->wheel.bitSize) <= lenbits)
		st->wheel = clamp8(read_signed(report, layout->wheel.bitOffset, layout->wheel.bitSize));
	if (layout->hwheel.valid &&
	    (unsigned short)(layout->hwheel.bitOffset + layout->hwheel.bitSize) <= lenbits)
		st->hwheel = clamp8(read_signed(report, layout->hwheel.bitOffset, layout->hwheel.bitSize));
	return 1;
}
