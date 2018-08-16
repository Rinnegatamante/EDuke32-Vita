TARGET		:= EDuke32
TITLE		:= EDUKE0032
GIT_VERSION := $(shell git describe --abbrev=6 --dirty --always --tags)

INCLUDES := build/include build/src build/src/jmact build/src/jaudiolib/include build/src/enet/include

LIBS = -lvita2d -lvorbisfile -lvorbis -logg  -lspeexdsp -lmpg123 \
	-lc -lSceCommonDialog_stub -lSceAudio_stub -lSceLibKernel_stub \
	-lSceNet_stub -lSceNetCtl_stub -lpng -lz -lSceDisplay_stub -lSceGxm_stub -lSceHid_stub \
	-lSceSysmodule_stub -lSceCtrl_stub -lSceTouch_stub -lSceMotion_stub -lm -lSceAppMgr_stub \
	-lSceAppUtil_stub -lScePgf_stub -ljpeg -lSceRtc_stub -lScePower_stub -lSDL_mixer -lSDL -lvitaGL -lmikmod \
	-lspeexdsp

BUILD_SRC = \
	build/src/a-c.c \
	build/src/baselayer.c \
	build/src/cache1d.c \
	build/src/compat.c \
	build/src/common.c \
	build/src/crc32.c \
	build/src/defs.c \
	build/src/colmatch.c \
	build/src/engine.c \
	build/src/mdsprite.c \
	build/src/texcache.c \
	build/src/dxtfilter.c \
	build/src/hightile.c \
	build/src/textfont.c \
	build/src/smalltextfont.c \
	build/src/kplib.c \
	build/src/mmulti_null.c \
	build/src/lz4.c \
	build/src/osd.c \
	build/src/md4.c \
	build/src/pragmas.c \
	build/src/scriptfile.c \
	build/src/mutex.c \
	build/src/xxhash.c \
	build/src/voxmodel.c \
	build/src/ctrlayer.c

GAME_SRC=build/src/game.c \
	build/src/actors.c \
	build/src/anim.c \
	build/src/animsounds.c \
	build/src/animvpx.c \
	build/src/common.c \
	build/src/config.c \
	build/src/demo.c \
	build/src/gamedef.c \
	build/src/gameexec.c \
	build/src/gamevars.c \
	build/src/global.c \
	build/src/input.c \
	build/src/menus.c \
	build/src/namesdyn.c \
	build/src/net.c \
	build/src/player.c \
	build/src/premap.c \
	build/src/savegame.c \
	build/src/sector.c \
	build/src/rts.c \
	build/src/osdfuncs.c \
	build/src/osdcmds.c \
	build/src/grpscan.c \
	build/src/sounds.c \
	build/src/soundsdyn.c \
  	build/src/rev.c
	
MUSIC_SRC=build/src/audiodec/psp2music.cpp \
	build/src/audiodec/audio_decoder.cpp \
	build/src/audiodec/audio_resampler.cpp \
	build/src/audiodec/decoder_fmmidi.cpp \
	build/src/audiodec/midisequencer.cpp \
	build/src/audiodec/midisynth.cpp

JMACT_SRC=build/src/jmact/file_lib.c \
	build/src/jmact/control.c \
	build/src/jmact/keyboard.c \
	build/src/jmact/mouse.c \
	build/src/jmact/joystick.c \
	build/src/jmact/scriplib.c \
	build/src/jmact/animlib.c

JAUDIO_SRC= build/src/jaudiolib/src/drivers.c \
	build/src/jaudiolib/src/fx_man.c \
	build/src/jaudiolib/src/multivoc.c \
	build/src/jaudiolib/src/mix.c \
	build/src/jaudiolib/src/mixst.c \
	build/src/jaudiolib/src/pitch.c \
	build/src/jaudiolib/src/formats.c \
	build/src/jaudiolib/src/vorbis.c \
	build/src/jaudiolib/src/flac.c \
	build/src/jaudiolib/src/xa.c \
	build/src/jaudiolib/src/driver_nosound.c \
	build/src/jaudiolib/src/driver_sdl.c    
    
CFILES   := $(BUILD_SRC) $(GAME_SRC) $(JMACT_SRC) $(JAUDIO_SRC)
CPPFILES   := $(MUSIC_SRC)
BINFILES := $(foreach dir,$(DATA), $(wildcard $(dir)/*.bin))
OBJS     := $(addsuffix .o,$(BINFILES)) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)

export INCLUDE	:= $(foreach dir,$(INCLUDES),-I$(dir))

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CXX      = $(PREFIX)-g++
CFLAGS  = $(INCLUDE) -D__PSP2__ -DNOASM -std=c99 -DNETCODE_DISABLE \
        -DHAVE_VORBIS -mfpu=neon -mcpu=cortex-a9 -Wl,-q -O2 -g \
		-DWANT_FMMIDI=1 -DUSE_AUDIO_RESAMPLER -DHAVE_LIBSPEEXDSP
CXXFLAGS  = $(CFLAGS) -fno-exceptions -std=gnu++11
ASFLAGS = $(CFLAGS)

all: $(TARGET).vpk

$(TARGET).vpk: $(TARGET).velf
	vita-make-fself -s $< build/build/eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLE) -d ATTRIBUTE2=12 "$(TARGET)" param.sfo
	cp -f param.sfo build/build/sce_sys/param.sfo

	#------------ Comment this if you don't have 7zip ------------------
	7z a -tzip ./$(TARGET).vpk -r ./build/build/sce_sys ./build/build/eboot.bin
	#-------------------------------------------------------------------

%.velf: %.elf
	cp $< $<.unstripped.elf
	$(PREFIX)-strip -g $<
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

clean:
	@rm -rf $(TARGET).velf $(TARGET).elf $(OBJS) $(TARGET).elf.unstripped.elf $(TARGET).vpk build/eboot.bin build/sce_sys/param.sfo ./param.sfo
