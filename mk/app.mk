# Common targets for one Flipper app directory (a directory with application.fam).
#
# Use from an app Makefile:
#
#     MK_ROOT := ../../mk        # path to this directory
#     include $(MK_ROOT)/app.mk
#
# Optional variables, set before the include:
#     TEST_DIR        host test directory (default: test)
#     TEST_GOAL       make goal that runs the host tests there (default: check)
#     TEST_CLEAN_GOAL make goal that cleans them (default: clean)
#     HOST_TEST_CMD   shell command that replaces "make -C $(TEST_DIR) $(TEST_GOAL)"
#     BUILD_CMD       shell command that replaces "ufbt" for apps that need a
#                     full firmware tree instead of the uFBT SDK
#     LAUNCH_CMD      shell command that replaces "ufbt launch"
#     UFBT            ufbt executable (default: ufbt, expects ~/.local/bin on PATH)
#     UFBT_CHANNEL    SDK channel for "make update" (default: release)
#
# Targets: all (build + test), build, test, lint, format, launch/upload, clean,
# update, cdb, help.

UFBT ?= ufbt
UFBT_CHANNEL ?= release
TEST_DIR ?= test
TEST_GOAL ?= check
TEST_CLEAN_GOAL ?= clean
APP_NAME := $(notdir $(CURDIR))

.DEFAULT_GOAL := all
.PHONY: all build test lint format launch upload clean update cdb help

all: build test

## build: compile the .fap into dist/ with ufbt
build:
ifdef BUILD_CMD
	$(BUILD_CMD)
else
	$(UFBT)
endif

## test: run the host unit tests (no Flipper or SDK needed)
test:
ifdef HOST_TEST_CMD
	$(HOST_TEST_CMD)
else ifneq ($(wildcard $(TEST_DIR)/Makefile),)
	$(MAKE) -C $(TEST_DIR) $(TEST_GOAL)
else
	@echo "$(APP_NAME): no host tests"
endif

## lint: clang-format check via ufbt
lint:
	$(UFBT) lint

## format: clang-format the sources in place via ufbt
format:
	$(UFBT) format

## launch / upload: build, upload over USB and start the app on a connected Flipper
launch upload:
ifdef LAUNCH_CMD
	$(LAUNCH_CMD)
else
	$(UFBT) launch
endif

## update: fetch the SDK for UFBT_CHANNEL (release by default)
update:
	$(UFBT) update --channel=$(UFBT_CHANNEL)

## cdb: regenerate compile_commands.json for the IDE
cdb:
	$(UFBT) cdb

## clean: remove ufbt build output, dist/ and host test binaries
clean:
	-$(UFBT) -c >/dev/null 2>&1
	rm -rf dist
ifeq ($(HOST_TEST_CMD),)
ifneq ($(wildcard $(TEST_DIR)/Makefile),)
	-$(MAKE) -C $(TEST_DIR) $(TEST_CLEAN_GOAL)
endif
endif

help:
	@echo "$(APP_NAME) targets:"
	@grep -h '^## ' $(MAKEFILE_LIST) | sed 's/^## /  /'
