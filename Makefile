TARGET		:= EDuke32
TITLE		:= EDUKE0032

INCLUDES := include src src/jmact src/jaudiolib/include src/enet/include

LIBS = -lvita2d -lvorbisfile -lvorbis -logg  -lspeexdsp -lmpg123 \
	-lc -lSceCommonDialog_stub -lSceAudio_stub -lSceLibKernel_stub \
	-lSceNet_stub -lSceNetCtl_stub -lpng -lz -lSceDisplay_stub -lSceGxm_stub -lSceHid_stub \
	-lSceSysmodule_stub -lSceCtrl_stub -lSceTouch_stub -lSceMotion_stub -lm -lSceAppMgr_stub \
	-lSceAppUtil_stub -lScePgf_stub -ljpeg -lSceRtc_stub -lScePower_stub -lSDL_mixer -lSDL -lvitaGL -lmikmod \
	-lspeexdsp

BUILD_SRC = \
	src/a-c.c \
	src/baselayer.c \
	src/cache1d.c \
	src/compat.c \
	src/common.c \
	src/crc32.c \
	src/defs.c \
	src/colmatch.c \
	src/engine.c \
	src/mdsprite.c \
	src/texcache.c \
	src/dxtfilter.c \
	src/hightile.c \
	src/textfont.c \
	src/smalltextfont.c \
	src/kplib.c \
	src/mmulti_null.c \
	src/lz4.c \
	src/osd.c \
	src/md4.c \
	src/pragmas.c \
	src/scriptfile.c \
	src/mutex.c \
	src/xxhash.c \
	src/voxmodel.c \
	src/psp2layer.c

GAME_SRC=src/game.c \
	src/actors.c \
	src/anim.c \
	src/animsounds.c \
	src/animvpx.c \
	src/common.c \
	src/config.c \
	src/demo.c \
	src/gamedef.c \
	src/gameexec.c \
	src/gamevars.c \
	src/global.c \
	src/input.c \
	src/menus.c \
	src/namesdyn.c \
	src/net.c \
	src/player.c \
	src/premap.c \
	src/savegame.c \
	src/sector.c \
	src/rts.c \
	src/osdfuncs.c \
	src/osdcmds.c \
	src/grpscan.c \
	src/sounds.c \
	src/soundsdyn.c \
  	src/rev.c
	
MUSIC_SRC=src/audiodec/psp2music.cpp \
	src/audiodec/audio_decoder.cpp \
	src/audiodec/audio_resampler.cpp \
	src/audiodec/decoder_fmmidi.cpp \
	src/audiodec/midisequencer.cpp \
	src/audiodec/midisynth.cpp

JMACT_SRC=src/jmact/file_lib.c \
	src/jmact/control.c \
	src/jmact/keyboard.c \
	src/jmact/mouse.c \
	src/jmact/joystick.c \
	src/jmact/scriplib.c \
	src/jmact/animlib.c

JAUDIO_SRC= src/jaudiolib/src/drivers.c \
	src/jaudiolib/src/fx_man.c \
	src/jaudiolib/src/multivoc.c \
	src/jaudiolib/src/mix.c \
	src/jaudiolib/src/mixst.c \
	src/jaudiolib/src/pitch.c \
	src/jaudiolib/src/formats.c \
	src/jaudiolib/src/vorbis.c \
	src/jaudiolib/src/flac.c \
	src/jaudiolib/src/xa.c \
	src/jaudiolib/src/driver_nosound.c \
	src/jaudiolib/src/driver_sdl.c    
    
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
	vita-make-fself -s $< build/eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLE) -d ATTRIBUTE2=12 "$(TARGET)" param.sfo
	cp -f param.sfo build/sce_sys/param.sfo

	#------------ Comment this if you don't have 7zip ------------------
	7z a -tzip ./$(TARGET).vpk -r ./build/sce_sys ./build/eboot.bin
	#-------------------------------------------------------------------

%.velf: %.elf
	cp $< $<.unstripped.elf
	$(PREFIX)-strip -g $<
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

clean:
	@rm -rf $(TARGET).velf $(TARGET).elf $(OBJS) $(TARGET).elf.unstripped.elf $(TARGET).vpk build/eboot.bin build/sce_sys/param.sfo ./param.sfo
