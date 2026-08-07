#
# DevilutionX as a bootable Raspberry Pi kernel image.
#
# One board per invocation (RAPI_BOARD, default rpi5). The board selects
# three things at once: the circle-stdlib world the kernel is compiled and
# linked against, the shim archive built against that world, and — through
# the world's Config.mk — the CPU tuning and RASPPI value. Each board's
# newlib and libc++ are built for its own architecture, so an image is
# never portable between boards.
#
#   make                     the default board's image
#   make RAPI_BOARD=rpi3     that board's image
#   make clean-board         drop just this board's build/<board>/ tree
#
# Everything this invocation generates lands under build/<board>/: the
# kernel's own objects, every DevilutionX object, every third-party object,
# and the final .elf/.map/.lst/.img. The object names are shared across
# boards but their contents are not (different -mcpu, different -DRASPPI,
# different world), so a flat build directory would silently relink one
# board's objects into another board's kernel — a corrupt image with no
# build error. A directory per board keeps all three isolated: no
# `make clean` between boards, and three boards can build at the same time
# without touching each other.
#
# The upstream trees — ../devilutionX and everything under ../deps — are
# never written into: they are read-only input, and every object they
# produce is redirected here.
#
# Prerequisite: the board's world and shim archive exist —
# `make -C ../circle-libsdl2 deps`, or `make deps` at the repository root.
#

include ../mk/toolchain.mk

BOARDS      = rpi3 rpi4 rpi5
RAPI_BOARD ?= rpi5

ROOT = ..
# Overridable so a second checkout of the shim can be pointed at without
# editing this file. The default is this repository's own submodule, which
# is what a plain clone gets.
SHIM ?= $(ROOT)/circle-libsdl2

DVL     = $(ROOT)/devilutionX
DEPSDIR = $(ROOT)/deps

OBJDIR = build/$(RAPI_BOARD)

# Circle's own image names, which the Raspberry Pi firmware selects between
# from the model sections of config.txt: the Pi 3 world boots kernel8.img,
# the Pi 4 world kernel8-rpi4.img, the Pi 5 world kernel_2712.img.
IMAGE_rpi3 = kernel8
IMAGE_rpi4 = kernel8-rpi4
IMAGE_rpi5 = kernel_2712
IMAGE = $(IMAGE_$(RAPI_BOARD))

# Before the world is included, so an unknown board is named as an unknown
# board. After the include it arrives as a missing Config.mk under a directory
# whose name nobody recognises.
ifeq ($(filter $(RAPI_BOARD),$(BOARDS)),)
$(error unknown RAPI_BOARD '$(RAPI_BOARD)' — expected one of: $(BOARDS))
endif

# The board's circle-stdlib world. A `make deps` that has not been run, or has
# not finished, leaves this file absent; make would then report that it has no
# rule to make a Config.mk, which describes the symptom and not the cause.
# WHERE THE BUILT WORLD COMES FROM.
#
# The default is this game's own nested shim, so a clone of this repository
# alone builds and uses its own worlds and nothing needs setting. A parent
# that keeps ONE set of built worlds for several games — they are gigabytes
# each, and identical when the pin and the configuration match — points
# CIRCLE_WORLDS at them.
CIRCLE_WORLDS ?= $(SHIM)
WORLD = $(CIRCLE_WORLDS)/circle-stdlib-$(RAPI_BOARD)
ifeq ($(wildcard $(WORLD)/Config.mk),)
$(error the $(RAPI_BOARD) circle-stdlib world is not configured ($(WORLD)/Config.mk is missing) — run 'make deps-$(RAPI_BOARD)' at the repository root)
endif

include $(WORLD)/Config.mk

TARGET = $(OBJDIR)/$(IMAGE)

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := image

# ---------------------------------------------------------------------------
# Sources: this port's own
# ---------------------------------------------------------------------------

HOST_SRCS = main.cpp kernel.cpp defaults.cpp circle_syscalls.cpp \
	circle_stubs.cpp
HOST_OBJS = $(addprefix $(OBJDIR)/,$(HOST_SRCS:.cpp=.o))

# ---------------------------------------------------------------------------
# Sources: DevilutionX
# ---------------------------------------------------------------------------
#
# The libdevilutionx source list, exactly as upstream's Source/CMakeLists.txt
# enumerates it, for this build's configuration:
#
#   NONET             no network stack exists under this board, so dvlnet's
#                     TCP transport, ZeroTier and packet encryption are all
#                     out. Upstream's loopback provider remains, which is
#                     what single player runs on.
#   NOSOUND           circle-libsdl2 implements SDL's audio device but not
#                     SDL_audiolib, which is the mixer DevilutionX decodes
#                     and mixes with. Upstream carries a build for exactly
#                     this case: effects_stubs.cpp and engine/sound_stubs.cpp
#                     replace the real ones and every sound call becomes a
#                     no-op, so the game runs silent rather than pretending.
#   SUPPORTS_MPQ      on. The game data is MPQ archives and stays that way.
#   DISABLE_DEMOMODE  off, so engine/demomode.cpp is built.
#
# Upstream's Source/main.cpp is NOT in this list. It is the executable's own
# source rather than part of libdevilutionx, and it is compiled separately
# below with its main() renamed, because main() in this image belongs to the
# Circle kernel.
DVL_CORE = \
	appfat.cpp automap.cpp capture.cpp codec.cpp control.cpp cursor.cpp \
	dead.cpp debug.cpp diablo.cpp doom.cpp engine.cpp error.cpp \
	gamemenu.cpp gmenu.cpp help.cpp hwcursor.cpp init.cpp interfac.cpp \
	inv.cpp itemdat.cpp items.cpp lighting.cpp loadsave.cpp menu.cpp \
	minitext.cpp misdat.cpp missiles.cpp monstdat.cpp monster.cpp \
	movie.cpp msg.cpp multi.cpp nthread.cpp objdat.cpp objects.cpp \
	options.cpp pack.cpp pfile.cpp player.cpp playerdat.cpp plrmsg.cpp \
	portal.cpp quests.cpp restrict.cpp sha.cpp spelldat.cpp spells.cpp \
	stores.cpp sync.cpp textdat.cpp tmsg.cpp towners.cpp track.cpp

DVL_CONTROLS = $(addprefix controls/, \
	axis_direction.cpp controller.cpp controller_buttons.cpp \
	controller_motion.cpp devices/joystick.cpp devices/kbcontroller.cpp \
	devices/game_controller.cpp game_controls.cpp menu_controls.cpp \
	modifier_hints.cpp plrctrls.cpp \
	touch/event_handlers.cpp touch/gamepad.cpp touch/renderers.cpp)

DVL_UI = $(addprefix DiabloUI/, \
	button.cpp credits.cpp credits_lines.cpp diabloui.cpp dialogs.cpp \
	hero/selhero.cpp mainmenu.cpp multi/selconn.cpp multi/selgame.cpp \
	progress.cpp scrollbar.cpp selok.cpp selstart.cpp selyesno.cpp \
	settingsmenu.cpp support_lines.cpp text_input.cpp title.cpp)

DVL_NET = $(addprefix dvlnet/, \
	abstract_net.cpp base.cpp cdwrap.cpp frame_queue.cpp loopback.cpp \
	packet.cpp)

DVL_ENGINE = $(addprefix engine/, \
	actor_position.cpp animationinfo.cpp assets.cpp backbuffer_state.cpp \
	demomode.cpp direction.cpp dx.cpp events.cpp load_cel.cpp \
	load_cl2.cpp load_clx.cpp load_pcx.cpp palette.cpp path.cpp \
	random.cpp sound_position.cpp sound_stubs.cpp surface.cpp trn.cpp \
	render/automap_render.cpp render/clx_render.cpp render/dun_render.cpp \
	render/primitive_render.cpp render/scrollrt.cpp render/text_render.cpp)

DVL_LEVELS = $(addprefix levels/, \
	crypt.cpp drlg_l1.cpp drlg_l2.cpp drlg_l3.cpp drlg_l4.cpp \
	gendung.cpp setmaps.cpp themes.cpp town.cpp trigs.cpp)

DVL_PANELS = $(addprefix panels/, \
	charpanel.cpp info_box.cpp mainpanel.cpp spell_book.cpp \
	spell_icons.cpp spell_list.cpp)

DVL_QOL = $(addprefix qol/, \
	autopickup.cpp chatlog.cpp floatingnumbers.cpp itemlabels.cpp \
	monhealthbar.cpp stash.cpp xpbar.cpp)

DVL_UTILS = $(addprefix utils/, \
	cel_to_clx.cpp cl2_to_clx.cpp console.cpp display.cpp file_util.cpp \
	format_int.cpp language.cpp logged_fstream.cpp parse_int.cpp \
	paths.cpp pcx_to_clx.cpp sdl_bilinear_scale.cpp sdl_thread.cpp \
	str_cat.cpp str_case.cpp surface_to_clx.cpp utf8.cpp)

DVL_MPQ = $(addprefix mpq/, \
	mpq_reader.cpp mpq_sdl_rwops.cpp mpq_writer.cpp)

DVL_SRCS = $(addprefix Source/, \
	$(DVL_CORE) $(DVL_CONTROLS) $(DVL_UI) $(DVL_NET) $(DVL_ENGINE) \
	$(DVL_LEVELS) $(DVL_PANELS) $(DVL_QOL) $(DVL_UTILS) $(DVL_MPQ) \
	items/validation.cpp platform/locale.cpp \
	storm/storm_net.cpp storm/storm_svid.cpp \
	effects_stubs.cpp encrypt.cpp) \
	3rdParty/PKWare/explode.cpp 3rdParty/PKWare/implode.cpp

DVL_OBJS = $(addprefix $(OBJDIR)/dvl/,$(DVL_SRCS:.cpp=.o)) \
	$(OBJDIR)/dvl/Source/main.o \
	$(OBJDIR)/dvl/3rdParty/bzip2/bz_internal_error.o

# ---------------------------------------------------------------------------
# Sources: the third-party libraries DevilutionX links against
# ---------------------------------------------------------------------------
#
# Upstream's CMake fetches each of these at configure time. There is no
# network during this build and no CMake in it, so each one is a submodule of
# this repository instead, pinned at the commit upstream's
# 3rdParty/<name>/CMakeLists.txt names. The file lists below are that same
# CMakeLists' lists.
#
# zlib and bzip2 are libmpq's decompressors. Diablo's own archives use
# neither — they are PKWARE-compressed, which is 3rdParty/PKWare above — but
# libmpq calls into both unconditionally, so both have to be present.

FMT_SRCS   = src/format.cc
FMT_OBJS   = $(addprefix $(OBJDIR)/deps/fmt/,$(FMT_SRCS:.cc=.o))

MPQ_SRCS   = $(addprefix libmpq/,common.c explode.c extract.c huffman.c mpq.c wave.c)
MPQ_OBJS   = $(addprefix $(OBJDIR)/deps/libmpq/,$(MPQ_SRCS:.c=.o))

SMK_SRCS   = $(addprefix src/,BitReader.cpp FileStream.cpp LogError.cpp SmackerDecoder.cpp)
SMK_OBJS   = $(addprefix $(OBJDIR)/deps/libsmackerdec/,$(SMK_SRCS:.cpp=.o))

# zlib's gz* files — the gzip-file layer over stdio — are deliberately not
# here. Nothing in this image opens a .gz: libmpq calls compress/uncompress
# on memory buffers and nothing else. They are also the only part of zlib
# that reaches for read/write/lseek/close directly, which on newlib needs a
# feature macro this build has no other reason to set. A reference to gzopen
# from anywhere would be an undefined symbol at link time, which is the
# honest way to find out this decision was wrong.
ZLIB_SRCS  = adler32.c compress.c crc32.c deflate.c \
	infback.c inffast.c inflate.c inftrees.c trees.c \
	uncompr.c zutil.c
ZLIB_OBJS  = $(addprefix $(OBJDIR)/deps/zlib/,$(ZLIB_SRCS:.c=.o))

BZIP2_SRCS = blocksort.c huffman.c crctable.c randtable.c compress.c \
	decompress.c bzlib.c
BZIP2_OBJS = $(addprefix $(OBJDIR)/deps/bzip2/,$(BZIP2_SRCS:.c=.o))

DEP_OBJS = $(FMT_OBJS) $(MPQ_OBJS) $(SMK_OBJS) $(ZLIB_OBJS) $(BZIP2_OBJS)

OBJS = $(HOST_OBJS) $(DVL_OBJS) $(DEP_OBJS)

# DevilutionX is written to C++20 (its CMakeLists says so, and fmt's
# compile-time format-string checking is the reason). The host layer is
# compiled the same way so one standard covers the whole image.
STANDARD = -std=gnu++23

# Stated, not inherited: this is Circle's own default, written down so the
# build's optimization level is a fact in the file rather than a forensic
# reconstruction.
OPTIMIZE = -O2

# Dependency files are written as a side effect of compiling rather than by
# rules of their own, and Circle's Rules.mk is told to keep its hands off
# (CHECK_DEPS) so its `-M -MG` .d rules are not defined for these objects.
#
# -MG lists a header make has no rule for as a prerequisite of the object,
# which stops the build; GNU make 3.81 stops with exit 2 and prints not one
# line saying why, which is why this build asks for 4.0 in mk/toolchain.mk. A
# .d left over from an earlier build naming a header that has since moved or
# been deleted stops it the same way, and is why "delete the board's object
# directory" kept being the cure.
#
# -MD writes the dependency file alongside the object it belongs to, so the
# two can never disagree about the flags that produced them, and -MP adds a
# rule with no recipe for every header named, so a header that disappears
# means a rebuild instead of a dead stop.
DEPS       = $(OBJS:.o=.d)
CHECK_DEPS = 0
DEPFLAGS   = -MD -MP

include $(CIRCLEHOME)/Rules.mk

# The world must be the multicore one circle-libsdl2 configures: the shim's
# core-split machinery cannot exist without it, and an object compiled
# against a single-core world links without complaint and then misbehaves.
# Every object here inherits the flag through Rules.mk's CPPFLAGS, so this
# check covers the whole build.
ifeq ($(findstring ARM_ALLOW_MULTI_CORE,$(DEFINE)),)
$(error the payload requires the multicore circle-stdlib world — build it with 'make -C ../circle-libsdl2 deps')
endif

# WHERE THIS GAME LIVES ON THE CARD. One card carries several games, and two
# of them writing a `diablo.ini` or a save game into the FAT root would each
# silently overwrite the other's. Every game therefore keeps to a directory
# of its own, and nothing of this port's ever touches the root.
#
# It is a build-time constant, not a boot switch: a card's layout is decided
# when the card is staged, and a game that could be relocated at boot would
# only mean two places to look for the same file.
#
# The path is used ONLY by this port's own host layer — argv, the working
# directory, and SDL's base and preference paths. Upstream DevilutionX is
# never told about it and needs no patch: it is simply pointed, with its own
# --data-dir, --save-dir and --config-dir switches.
RAPI_GAME_DIR ?= /games/devilutionx
DEFINE += -DRAPI_GAME_DIR='"$(RAPI_GAME_DIR)"'

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
#
# DevilutionX writes `#include <SDL.h>`, the spelling a desktop sdl2-config
# include path provides, so the shim's SDL2/ directory goes on the path in
# its own right alongside the SDL2/-prefixed one this project's own code
# uses.
#
# dvlext/ carries config.h, which upstream's own build system generates while
# configuring and which this port writes down instead.
#
# The rest are the include directories upstream's CMake targets export:
# Source/ for the game, 3rdParty/tl, 3rdParty/hoehrmann_utf8 and
# 3rdParty/PKWare for the headers vendored in the game's own tree, and one
# per fetched dependency.
INCLUDE := -I $(SHIM)/include -I $(SHIM)/include/SDL2 \
	-I $(CURDIR)/dvlext \
	-I $(DVL)/Source \
	-I $(DVL)/3rdParty/tl -I $(DVL)/3rdParty/hoehrmann_utf8 \
	-I $(DVL)/3rdParty/PKWare \
	-I $(DEPSDIR)/fmt/include -I $(DEPSDIR)/simpleini \
	-I $(DEPSDIR)/libsmackerdec/include -I $(DEPSDIR)/libmpq \
	$(CIRCLE_STDLIB_INCLUDES) $(INCLUDE)

# ---------------------------------------------------------------------------
# What this build of DevilutionX is
# ---------------------------------------------------------------------------
#
# Upstream turns its CMake options into preprocessor defines in
# CMake/Definitions.cmake; these are that translation for this build's
# configuration, and the reasons are with the source list above.
#
# The palette transparency lookup table is upstream's own default (on): it
# spends 128 KiB of RAM to make blending transparent black overlays cheaper,
# which is a trade this board is happy to make.
DVL_DEFINE = \
	-DNONET -DDISABLE_TCP -DDISABLE_ZERO_TIER \
	-DNOSOUND \
	-DDEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT \
	-DDEVILUTIONX_RESAMPLER_SPEEX -DDEVILUTIONX_DEFAULT_RESAMPLER=Speex \
	-DDEVILUTIONX_DISPLAY_TEXTURE_FORMAT=SDL_PIXELFORMAT_ARGB8888 \
	-D_POSIX_C_SOURCE=200809L

# THE TEXTURE FORMAT IS NOT UPSTREAM'S DEFAULT. Upstream asks for
# SDL_PIXELFORMAT_RGB888, which circle-libsdl2 refuses: the library creates
# streaming textures in ARGB8888 and in nothing else, and says so rather than
# silently converting. The two layouts are the same 32 bits with the same
# byte order; only the top byte's meaning differs.
#
# The resampler is named even though this build has no sound. Upstream's
# Resampler enum has no members at all unless one resampler is declared, and
# options.cpp names the default unconditionally — so the pair below is what
# makes the settings code compile. With NOSOUND nothing ever reads it.

# __FILE__ shortened to a path relative to the game's own tree, which is what
# upstream's build does and what its log lines are written to expect. Without
# it every logged file name would carry the whole build path of whatever
# machine produced the image.
DVL_DEFINE += -fmacro-prefix-map=$(abspath $(DVL))/=

# `-include new` is a workaround for an upstream bug, not a choice.
# Source/utils/static_vector.hpp calls std::launder without including <new>,
# and gets away with it on a desktop only because some other header in the
# chain has already brought it in. libc++ on this world does not, and the
# error names 24 translation units. Forcing the header in from the command
# line fixes it here without touching the upstream tree; upstream should add
# the include.
DVL_CXXFLAGS = $(DVL_DEFINE) -include new $(CPPFLAGS)

# The C dependencies (zlib, bzip2, libmpq) are C89/C99 code. Stated here
# rather than added to CFLAGS because Rules.mk folds CFLAGS into the C++
# flags as well, and a C standard handed to the C++ compiler is a warning on
# every C++ file in the build.
DEP_CFLAGS = -std=gnu99 $(RAPI_CFLAGS) $(CFLAGS)

# libmpq's own config.h lives in the game's tree, not in libmpq's: upstream
# supplies it from 3rdParty/libmpq/ and puts that directory first on the
# include path for libmpq's sources alone.
MPQ_CFLAGS = -std=gnu99 $(RAPI_CFLAGS) -I $(DVL)/3rdParty/libmpq -I $(DEPSDIR)/zlib -I $(DEPSDIR)/bzip2 \
	$(DEP_CFLAGS)

# The shim comes in whole, every member, rather than only the members the
# link happens to need. An object file linked straight into the kernel beats
# an archive member of the same name and does it silently, so a stub in this
# port's own layer left behind after the library implemented the real thing
# would quietly win — the game would keep calling the stub and nothing would
# say so. Pulling the archive in whole turns that into a duplicate-symbol
# error at link time, which is the only moment it can still be cheap to fix.
SHIM_LIB = $(SHIM)/libSDL2-$(RAPI_BOARD).a

LIBS = --whole-archive $(SHIM_LIB) --no-whole-archive \
	$(CIRCLE_STDLIB_LIBS)

# The shim archive is built in circle-libsdl2 and never here — it is a shared
# dependency, and a rule here would rebuild it at whatever board and crossing
# count this makefile happens to be running with. This rule exists only for the
# case where it is absent: it runs then, and only then, because a target with a
# recipe and no prerequisites is up to date as soon as the file exists.
$(SHIM_LIB):
	@echo "$@ is missing: the $(RAPI_BOARD) shim archive has not been built."
	@echo "Run: $(MAKE) -C $(SHIM) libSDL2-$(RAPI_BOARD).a BOARD=$(RAPI_BOARD)"
	@exit 1

# ---------------------------------------------------------------------------
# Filesystem syscalls, redirected at link time
# ---------------------------------------------------------------------------
#
# The game runs on the application core, and newlib's Circle glue drives
# FatFs and the SD card straight from wherever it is called — which off core
# 0 is illegal. circle_syscalls.cpp puts the shim's any-core I/O service
# underneath the C library instead; these flags are what put it there.
#
# --wrap rather than redefinition, because redefinition cannot work here:
# the glue defines _open, _read, _write, _close, _lseek, _fstat, opendir,
# readdir, closedir, dup and more in a SINGLE object file. Redefining part
# of that set either collides at link time or, worse, leaves the originals
# linked in beside the replacements with no diagnostic. --wrap renames the
# references instead and leaves the vendored glue untouched — and it is what
# keeps __real_* reachable, which is how the service performs the real work
# once it has reached core 0.
#
# Every name here must also appear in circle_syscalls.cpp: a wrap with no
# __wrap_ definition is an undefined reference, so the link is the check.
WRAPPED_SYSCALLS = \
	_open _close _read _write _lseek _fstat _stat lstat _unlink _rename \
	_fcntl mkdir chdir getcwd dup dup2 \
	opendir readdir closedir rewinddir

# The link is driven by the cross ld directly (Circle's Rules.mk sets
# LD = <prefix>ld for GCC builds), so these are plain linker options, not
# compiler-driver -Wl, ones.
LDFLAGS += $(addprefix --wrap=,$(WRAPPED_SYSCALLS))

# ---------------------------------------------------------------------------
# Compilation
# ---------------------------------------------------------------------------

# This project's own objects, board-scoped. These more-specific rules win
# over Rules.mk's in-place %.o/%.d, so nothing is written beside the source.
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) $(DEPFLAGS) -c -o $@ $<

# DevilutionX's objects, mirroring its source tree under $(OBJDIR)/dvl/.
# The source tree itself is the upstream submodule, and is never written to.
$(OBJDIR)/dvl/%.o: $(DVL)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CPP   $@"
	@$(CPP) $(DVL_CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/dvl/%.o: $(DVL)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@"
	@$(CC) $(DEP_CFLAGS) $(DEPFLAGS) -c -o $@ $<

# DevilutionX's main() is the program's entry point on a desktop, but here
# main() belongs to the Circle kernel (main.cpp in this directory), which
# calls the game once the board is up. Renaming the entry point through the
# preprocessor for this one translation unit keeps upstream's source
# untouched, and the kernel calls the new name.
$(OBJDIR)/dvl/Source/main.o: $(DVL)/Source/main.cpp
	@mkdir -p $(dir $@)
	@echo "  CPP   $@ (entry point renamed)"
	@$(CPP) $(DVL_CXXFLAGS) $(DEPFLAGS) -Dmain=devilutionx_main -c -o $@ $<

# The third-party dependencies, each mirrored under $(OBJDIR)/deps/<name>/.
$(OBJDIR)/deps/fmt/%.o: $(DEPSDIR)/fmt/%.cc
	@mkdir -p $(dir $@)
	@echo "  CPP   $@"
	@$(CPP) $(DVL_CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/deps/libsmackerdec/%.o: $(DEPSDIR)/libsmackerdec/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CPP   $@"
	@$(CPP) $(DVL_CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/deps/libmpq/%.o: $(DEPSDIR)/libmpq/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@"
	@$(CC) $(MPQ_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/deps/zlib/%.o: $(DEPSDIR)/zlib/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@"
	@$(CC) $(DEP_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR)/deps/bzip2/%.o: $(DEPSDIR)/bzip2/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@"
	@$(CC) $(DEP_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJDIR):
	@mkdir -p $(OBJDIR)

-include $(DEPS)

# The image link rule. sdl-app.mk lets the caller choose the linker script,
# so this build supplies its own: the shim's script with room made at image
# offset 0x800 for the patchable-defaults block. Everything else about the
# script — including the TLS-section adjacency binutils 2.44 and later
# require — is inherited from the shim's.
SDL_APP_LDSCRIPT = $(CURDIR)/devilutionx-defaults.ld
include $(SHIM)/sdl-app.mk

# The other half of the guarantee the linker script's ASSERTs begin: check
# the BYTES of the finished image, not just the link. A pre-boot writer
# refuses to stamp an image whose magic is not at 0x800, so an image that
# lost it would boot and then quietly ignore everything ever sent to it —
# including --rapi-debug-uart, which is the channel used to drive the board
# when nobody is at the bench. Better to have no image than that one.
.PHONY: image
image: $(TARGET).img
	@if [ "`dd if=$(TARGET).img bs=4 skip=512 count=1 2>/dev/null`" = "PM8D" ]; \
	then \
		echo "  GATE  $(TARGET).img defaults block at 0x800: present"; \
	else \
		echo "  GATE  $(TARGET).img: defaults block ABSENT at 0x800 — image deleted"; \
		rm -f $(TARGET).img; \
		exit 1; \
	fi

# Board-scoped clean: drop just this board's tree, leaving the other two
# boards' images and objects untouched.
.PHONY: clean-board
clean-board:
	rm -rf $(OBJDIR)

# This board from nothing. An incremental build is a decision made from
# timestamps, and a timestamp says when a file was written rather than what is
# in it; anything read off the resulting image describes what the last build
# happened to put there. Take a measurement that has to be right off one of
# these.
.PHONY: rebuild
rebuild:
	+@$(NOT_DRY_RUN)
	@$(MAKE) clean-board RAPI_BOARD=$(RAPI_BOARD)
	@$(MAKE) image RAPI_BOARD=$(RAPI_BOARD)
