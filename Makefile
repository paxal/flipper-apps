# Repo-wide driver. Each app directory has its own Makefile built on mk/app.mk;
# this one fans the common targets out to every app.
#
#   make test                       host tests of every app (no SDK or device needed)
#   make build                      ufbt build of every app into <app>/dist/
#   make lint | format | clean | update
#   make games/icecube              build + test one app
#   make -C games/icecube launch    upload one app to a USB-connected Flipper

APPS := \
	grayscale_poc \
	games/babal \
	games/falldown \
	games/icecube \
	csc_debug \
	nfc_write_debug

.DEFAULT_GOAL := all
.PHONY: all build test lint format clean update list $(APPS)

all: build test

build test lint format clean update:
	@for app in $(APPS); do \
		echo "=== $$app: $@"; \
		$(MAKE) --no-print-directory -C $$app $@ || exit 1; \
	done

$(APPS):
	$(MAKE) -C $@

list:
	@printf '%s\n' $(APPS)
