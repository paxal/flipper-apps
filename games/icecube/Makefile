MK_ROOT := ../../mk
include $(MK_ROOT)/app.mk

## frames: render every game screen to test/frames/*.png on the host
.PHONY: frames levels
frames:
	$(MAKE) -C test frames

## levels: regenerate levels/*.c and preview PNGs from ice1_1/ICE.ENT
levels:
	python3 -I tools/gen_levels.py
