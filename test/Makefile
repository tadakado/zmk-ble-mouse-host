# Host-side unit tests for the hardware-independent HID parser.
# Run: make -C test
CC      = cc
CFLAGS  = -I../include -Wall -Wextra
BUILD   = build

.PHONY: test clean

test: | $(BUILD)
	$(CC) $(CFLAGS) test_hid_parser.c ../src/hid_parser.c -o $(BUILD)/test_hid
	./$(BUILD)/test_hid

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
