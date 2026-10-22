MK_ROOT := ../mk

# The host test for this app lives in the repo-level tests/ directory.
HOST_TEST_CMD := $(CC) -Wall -Wextra -std=c11 -I helpers -o /tmp/test_csc_parse \
	../tests/test_csc_parse.c helpers/csc_parse.c -lm && /tmp/test_csc_parse

# This app needs the Full BLE stack APIs of Moon Firmware, which the uFBT
# release SDK does not export. Build it from a Moon Firmware tree:
#     make build MOON_FW=/path/to/Moon-Firmware
# (the tree needs applications_user/csc_debug pointing at this directory, see README.md).
ifdef MOON_FW
BUILD_CMD := cd $(MOON_FW) && ./fbt COMPACT=1 DEBUG=0 fap_csc_debug
LAUNCH_CMD := cd $(MOON_FW) && ./fbt launch APPSRC=applications_user/csc_debug
else
BUILD_CMD := @echo "csc_debug: skipped, needs Moon Firmware (make build MOON_FW=/path/to/Moon-Firmware)"
LAUNCH_CMD := @echo "csc_debug: needs Moon Firmware (make launch MOON_FW=/path/to/Moon-Firmware)"; exit 1
endif

include $(MK_ROOT)/app.mk
