// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tadashi Kadowaki
/*
 * Host-side unit tests for hid_parser.c.
 * Build & run: make -C test    (or: cc -Iinclude test/test_hid_parser.c \
 *                                       src/hid_parser.c -o /tmp/test_hid && /tmp/test_hid)
 */
#include <stdio.h>
#include <zmk_ble_mouse_host/hid_parser.h>

static int failures = 0;

#define CHECK(cond, msg)                                                      \
	do {                                                                  \
		if (!(cond)) {                                                \
			printf("  FAIL: %s\n", msg);                          \
			failures++;                                           \
		}                                                             \
	} while (0)

/* Standard boot-style 3-button mouse, no report ID. Layout: [btn][x][y][wheel]. */
static const unsigned char boot_mouse[] = {
	0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
	0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
	0x95, 0x03, 0x75, 0x01, 0x81, 0x02, /* 3 buttons */
	0x95, 0x01, 0x75, 0x05, 0x81, 0x03, /* 5 const pad */
	0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
	0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, /* X,Y,Wheel 8-bit */
	0xC0, 0xC0
};

/* Real Logitech receiver mouse report (report ID 2): 16 buttons, 16-bit X/Y,
 * 8-bit wheel, 8-bit AC Pan (horizontal wheel). Captured from hardware. */
static const unsigned char report_id_mouse[] = {
	0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xA1, 0x00,
	0x95, 0x10, 0x75, 0x01, 0x15, 0x00, 0x25, 0x01,
	0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x81, 0x02, /* 16 buttons */
	0x95, 0x02, 0x75, 0x10, 0x16, 0x01, 0x80, 0x26, 0xFF, 0x7F,
	0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x81, 0x06, /* X,Y 16-bit */
	0x95, 0x01, 0x75, 0x08, 0x15, 0x81, 0x25, 0x7F, 0x09, 0x38, 0x81, 0x06, /* wheel */
	0x95, 0x01, 0x05, 0x0C, 0x0A, 0x38, 0x02, 0x81, 0x06, /* AC Pan (hwheel) */
	0xC0, 0xC0
};

static void test_boot_mouse(void)
{
	MouseLayout l;
	MouseState s;
	unsigned char rep[4];

	printf("test_boot_mouse\n");
	hid_parse_mouse(boot_mouse, sizeof(boot_mouse), &l);
	CHECK(l.hasReportId == 0, "boot mouse has no report id");
	CHECK(l.mouseReportId == 0, "mouseReportId == 0");
	CHECK(l.buttons.valid && l.buttons.bitOffset == 0, "buttons at bit 0");
	CHECK(l.x.valid && l.x.bitOffset == 8 && l.x.bitSize == 8, "X byte 1, 8-bit");
	CHECK(l.y.valid && l.y.bitOffset == 16, "Y byte 2");
	CHECK(l.wheel.valid && l.wheel.bitOffset == 24, "wheel byte 3");
	CHECK(!l.hwheel.valid, "no hwheel on boot mouse");

	/* buttons=0x05 (1+3), x=+10, y=-5, wheel=+1 */
	rep[0] = 0x05;
	rep[1] = 10;
	rep[2] = (unsigned char)(-5);
	rep[3] = 1;
	CHECK(hid_extract_mouse(&l, rep, sizeof(rep), &s) == 1, "extract ok");
	CHECK(s.buttons == 0x05, "buttons 0x05");
	CHECK(s.dx == 10, "dx 10");
	CHECK(s.dy == -5, "dy -5");
	CHECK(s.wheel == 1, "wheel 1");
	CHECK(s.hwheel == 0, "hwheel 0");
}

static void test_report_id_mouse(void)
{
	MouseLayout l;
	MouseState s;
	unsigned char rep[9];

	printf("test_report_id_mouse\n");
	hid_parse_mouse(report_id_mouse, sizeof(report_id_mouse), &l);
	CHECK(l.hasReportId == 1, "has report id");
	CHECK(l.mouseReportId == 2, "mouse report id 2");
	CHECK(l.buttons.valid && l.buttons.bitOffset == 8 && l.buttons.bitSize == 16,
	      "buttons 16 bits at byte 1");
	CHECK(l.x.valid && l.x.bitOffset == 24 && l.x.bitSize == 16, "X 16-bit byte 3");
	CHECK(l.y.valid && l.y.bitOffset == 40 && l.y.bitSize == 16, "Y 16-bit byte 5");
	CHECK(l.wheel.valid && l.wheel.bitOffset == 56 && l.wheel.bitSize == 8, "wheel byte 7");
	CHECK(l.hwheel.valid && l.hwheel.bitOffset == 64 && l.hwheel.bitSize == 8, "hwheel byte 8");

	/* report id 2, button1 down, X=+1, Y=0, wheel=0, hwheel=0 (live capture) */
	rep[0] = 0x02;
	rep[1] = 0x01; rep[2] = 0x00; /* buttons */
	rep[3] = 0x01; rep[4] = 0x00; /* X = +1 */
	rep[5] = 0x00; rep[6] = 0x00; /* Y = 0 */
	rep[7] = 0x00;                /* wheel */
	rep[8] = 0x00;                /* hwheel */
	CHECK(hid_extract_mouse(&l, rep, sizeof(rep), &s) == 1, "extract ok");
	CHECK(s.buttons == 0x01, "button1 down");
	CHECK(s.dx == 1, "dx +1");
	CHECK(s.dy == 0, "dy 0");

	/* buttons beyond the low 8 are preserved (16-bit). button 9 -> bit8 = 0x0100 */
	rep[1] = 0x00; rep[2] = 0x01;
	CHECK(hid_extract_mouse(&l, rep, sizeof(rep), &s) == 1, "extract ok");
	CHECK(s.buttons == 0x0100, "button 9 (bit8) preserved");
	rep[1] = 0x80; rep[2] = 0x80; /* button 8 + button 16 */
	CHECK(hid_extract_mouse(&l, rep, sizeof(rep), &s) == 1, "extract ok");
	CHECK(s.buttons == 0x8080, "buttons 8 and 16");
	rep[1] = 0x00; rep[2] = 0x00;

	/* wrong report id must be rejected */
	rep[0] = 0x03;
	CHECK(hid_extract_mouse(&l, rep, sizeof(rep), &s) == 0, "reject other report id");
}

static void test_boundaries(void)
{
	MouseLayout l;
	MouseState s;
	unsigned char rep[9];

	printf("test_boundaries\n");
	hid_parse_mouse(report_id_mouse, sizeof(report_id_mouse), &l);

	/* 16-bit X = +200, Y = -300 are preserved at full resolution */
	rep[0] = 0x02; rep[1] = 0; rep[2] = 0;
	rep[3] = (unsigned char)(200 & 0xFF); rep[4] = (unsigned char)((200 >> 8) & 0xFF);
	rep[5] = (unsigned char)((-300) & 0xFF); rep[6] = (unsigned char)(((-300) >> 8) & 0xFF);
	rep[7] = 0x7F; /* wheel +127 (8-bit) */
	rep[8] = 0x80; /* hwheel -128 (8-bit) */
	CHECK(hid_extract_mouse(&l, rep, sizeof(rep), &s) == 1, "extract ok");
	CHECK(s.dx == 200, "dx 16-bit +200 preserved");
	CHECK(s.dy == -300, "dy 16-bit -300 preserved");
	CHECK(s.wheel == 127, "wheel +127");
	CHECK(s.hwheel == -128, "hwheel -128");

	/* near full-scale 16-bit values */
	rep[3] = 0xFF; rep[4] = 0x7F; /* +32767 */
	rep[5] = 0x00; rep[6] = 0x80; /* -32768 */
	CHECK(hid_extract_mouse(&l, rep, sizeof(rep), &s) == 1, "extract ok");
	CHECK(s.dx == 32767, "dx +32767");
	CHECK(s.dy == -32768, "dy -32768");

	/* exact 16-bit -1 -> -1 */
	rep[3] = 0xFF; rep[4] = 0xFF;
	CHECK(hid_extract_mouse(&l, rep, sizeof(rep), &s) == 1, "extract ok");
	CHECK(s.dx == -1, "dx 16-bit 0xFFFF == -1");
}

static void test_invalid(void)
{
	MouseLayout l;
	unsigned char junk[] = {0xFF, 0x12, 0x34, 0x99, 0x00, 0x05}; /* not a mouse */

	printf("test_invalid\n");
	hid_parse_mouse(junk, sizeof(junk), &l);
	CHECK(!l.x.valid, "no X usage -> invalid layout");

	/* truncated descriptor: long item header without data bytes */
	{
		unsigned char trunc[] = {0x05, 0x01, 0x26}; /* 0x26 wants 2 data bytes */
		hid_parse_mouse(trunc, sizeof(trunc), &l);
		CHECK(!l.x.valid, "truncated -> invalid, no crash");
	}
}

int main(void)
{
	test_boot_mouse();
	test_report_id_mouse();
	test_boundaries();
	test_invalid();
	if (failures == 0) {
		printf("\nALL TESTS PASSED\n");
		return 0;
	}
	printf("\n%d TEST(S) FAILED\n", failures);
	return 1;
}
