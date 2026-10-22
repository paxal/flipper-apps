MK_ROOT := ../../mk
TEST_DIR := tests
TEST_GOAL := test
include $(MK_ROOT)/app.mk

## goldens: re-record the host render goldens after an intentional visual change
.PHONY: goldens
goldens:
	RECORD_GOLDEN=1 $(MAKE) -C tests record_golden
