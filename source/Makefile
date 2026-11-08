PS3DEV ?= /usr/local/ps3dev
PSL1GHT ?= $(PS3DEV)
PS3_OLD_LIBS ?= $(PS3DEV)/ppu/lib
PPU_CC ?= ppu-gcc
PPU_STRIP ?= ppu-strip
SPRX_LINKER ?= sprxlinker
SFO ?= sfo
PKG ?= pkg
MAKE_SELF_NPDRM ?= make_self_npdrm
PACKAGE_FINALIZE ?= package_finalize
SFO_XML ?= assets/sfo.xml
TITLE_ID ?= RFLP00001
APP_TITLE ?= Puffle 3 Emulator
TARGET ?= ruffle-ps3-dev
CONTENT_ID := UP0001-$(TITLE_ID)_00-0000000000000000
RSXKIT_OUT := $(abspath build/rsxkit)
SOURCES := src/ps3_main.c src/ps3me_ui.c src/rsx_renderer.c src/pixel_convert.c src/rust_runtime.c src/audio_output.c src/storage.c src/controls.c src/profile.c src/browser.c
CFLAGS_PS3 := -std=gnu11 -O2 -g -Wall -Wextra -Werror -mcpu=cell -mhard-float -ffunction-sections -fdata-sections
INCLUDES := -Iinclude -Ivendor/rsxkit/include -I$(PSL1GHT)/ppu/include -I$(PS3DEV)/ppu/include
RUST_LIB ?= player/target-ps3/powerpc64-sony-ps3/release/libruffle_ps3_player.a
LIBS := -L$(PSL1GHT)/ppu/lib -L$(PS3_OLD_LIBS) -Wl,--gc-sections -laudio -lrsx -lgcm_sys -lio -lfontFT -lfont -lsysutil -lnet -lsysmodule -lrt -llv2 -lm

.PHONY: all rsxkit package test clean
all: build/$(TARGET).elf
rsxkit:
	$(MAKE) -C vendor/rsxkit OUT=$(RSXKIT_OUT)
build/$(TARGET).raw.elf: $(SOURCES) include/controls.h include/ps3me_ui.h include/browser.h include/profile.h include/player.h include/pixel_convert.h include/rsx_profile.h include/frame_stats.h $(RUST_LIB) | rsxkit
	@mkdir -p build
	$(PPU_CC) $(CFLAGS_PS3) $(INCLUDES) $(SOURCES) $(RSXKIT_OUT)/librsxkit.a $(RUST_LIB) $(LIBS) -o $@
build/$(TARGET).elf: build/$(TARGET).raw.elf
	$(PPU_STRIP) $< -o $@
	$(SPRX_LINKER) $@
package: build/$(TARGET).elf assets/ICON0.PNG assets/PIC1.PNG
	@rm -rf build/pkg && mkdir -p build/pkg/USRDIR
	cp assets/ICON0.PNG build/pkg/ICON0.PNG
	cp assets/PIC1.PNG build/pkg/PIC1.PNG
	cp -r assets/ui build/pkg/USRDIR/ui
	python3 scripts/collect-licenses.py
	cp build/THIRD_PARTY_LICENSES.txt COPYING.PS3ME LICENSE NOTICE.md build/pkg/USRDIR/
	$(MAKE_SELF_NPDRM) build/$(TARGET).elf build/pkg/USRDIR/EBOOT.BIN $(CONTENT_ID)
	$(SFO) --title '$(APP_TITLE)' --appid $(TITLE_ID) -f $(SFO_XML) build/pkg/PARAM.SFO
	rm -f build/$(TARGET).pkg
	$(PKG) --contentid=$(CONTENT_ID) build/pkg/ build/$(TARGET).pkg
	$(PACKAGE_FINALIZE) build/$(TARGET).pkg
test:
	./scripts/test-host.sh
clean:
	rm -rf build
