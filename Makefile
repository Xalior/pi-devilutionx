#
# pi-devilutionx — DevilutionX as a bootable bare-metal Raspberry Pi image.
#
#   make check-toolchain     report the cross compiler this build will use
#   make deps                the three circle-stdlib worlds and the shim
#                            archives built against them (long: the worlds
#                            build newlib and libc++ from source)
#   make deps-rpi4           the same for one board only, for a machine that
#                            cannot hold three worlds at once
#   make rpi5 | rpi4 | rpi3  one board's kernel image
#   make kernels             all three, built in parallel
#   make verify              truth-gate: every image exists and is non-empty
#   make media               download the freely redistributable game data
#                            into media/. Run by a person, on their own
#                            responsibility — read the README first
#   make netboot             stage the Pi 5 image and its boot configuration
#                            into build/netboot-rpi5/
#   make card                stage the whole card into build/sd-card/, copying
#                            in whatever media/ holds and naming what it does
#                            not. It never downloads anything
#   make clean-boards        drop every board's build tree
#
# The three boards never share mutable state: each has its own circle-stdlib
# world, its own shim archive and its own object directory, so building them
# at the same time is safe and building one never disturbs another.
#
# The libc++ sources every world is built from are one immutable git tag, and
# CIRCLE_LLVM says where that checkout lives. The default puts it beside this
# repository, which is right for a plain clone and for a CI runner. Point
# several projects at one directory to fetch it once for all of them:
#
#   make deps CIRCLE_LLVM=/path/to/circle-llvm
#

include mk/toolchain.mk

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := kernels

BOARDS ?= rpi3 rpi4 rpi5

IMAGE_rpi3 = kernel8.img
IMAGE_rpi4 = kernel8-rpi4.img
IMAGE_rpi5 = kernel_2712.img

.PHONY: deps kernels rebuild verify media netboot card clean-boards $(BOARDS)
.PHONY: $(addprefix deps-,$(BOARDS)) $(addprefix rebuild-,$(BOARDS))

deps:
	+@$(NOT_DRY_RUN)
	$(MAKE) -C circle-libsdl2 deps

# One board's dependencies: its own circle-stdlib world and the shim archive
# built against it. A machine with a small disk — a CI runner, most obviously
# — builds one board at a time and keeps only that board's world.
# Written as a static pattern rule over the board list rather than a plain
# pattern rule: these targets are phony, and make does not apply pattern rules
# to phony targets — it would quietly answer "nothing to be done" and leave
# the world unbuilt.
$(addprefix deps-,$(BOARDS)): deps-%:
	+@$(NOT_DRY_RUN)
	$(MAKE) -C circle-libsdl2 world BOARD=$*
	$(MAKE) -C circle-libsdl2 libSDL2-$*.a BOARD=$*

$(BOARDS): check-toolchain
	+@$(NOT_DRY_RUN)
	$(MAKE) -C host RAPI_BOARD=$@

# All three at once. Each sub-make owns a different world and a different
# output directory, so there is nothing for them to collide on.
#
# Each board is waited for BY PID, and its status kept. A bare `wait` reports
# only that the shell has no children left — it is success whatever the jobs
# did — so a board that failed to build would leave this target reporting
# success, and the truth-gate would then pass the board's PREVIOUS image,
# still on disk.
kernels: check-toolchain
	+@$(NOT_DRY_RUN)
	@pids=; fail=0; \
	for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b & pids="$$pids $$!"; done; \
	for p in $$pids; do wait $$p || fail=1; done; \
	exit $$fail

# One board from nothing: its build tree is removed before the build, so no
# object can be inherited from a previous one. Written as a static pattern rule
# over the board list for the same reason deps-% is.
$(addprefix rebuild-,$(BOARDS)): rebuild-%: check-toolchain
	+@$(NOT_DRY_RUN)
	$(MAKE) -C host RAPI_BOARD=$* rebuild

# All three from nothing, in parallel, waited for by PID exactly as kernels is.
rebuild: check-toolchain
	+@$(NOT_DRY_RUN)
	@pids=; fail=0; \
	for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b rebuild & pids="$$pids $$!"; done; \
	for p in $$pids; do wait $$p || fail=1; done; \
	exit $$fail

# Truth-gate: ask the filesystem, not the exit codes. An image that is
# missing, empty, or does not carry the defaults block at offset 0x800 fails
# here even if the build claimed success.
#
# What this cannot tell you is whether the image was built from the sources as
# they now stand. That is a question about the build, not about the file, and
# `make rebuild` is the only answer to it.
verify:
	@fail=0; \
	for b in $(BOARDS); do \
		case $$b in \
			rpi3) img=host/build/rpi3/$(IMAGE_rpi3) ;; \
			rpi4) img=host/build/rpi4/$(IMAGE_rpi4) ;; \
			rpi5) img=host/build/rpi5/$(IMAGE_rpi5) ;; \
		esac; \
		if [ ! -s "$$img" ]; then \
			echo "  FAIL  $$img missing or empty"; fail=1; \
		elif [ "`dd if=$$img bs=4 skip=512 count=1 2>/dev/null`" != "PM8D" ]; then \
			echo "  FAIL  $$img has no defaults block at 0x800"; fail=1; \
		else \
			echo "  OK    $$img ($$(wc -c < $$img | tr -d ' ') bytes, defaults block present)"; \
		fi; \
	done; \
	exit $$fail

# ---------------------------------------------------------------------------
# Game data
# ---------------------------------------------------------------------------
#
# TWO DIRECTORIES, AND THE SEPARATION BETWEEN THEM IS THE POINT.
#
#   media/           what `make media` downloads. Gitignored, never shipped,
#                    and never part of a build.
#   build/sd-card/   what `make card` stages. It COPIES FROM media/ and
#                    fetches nothing, ever.
#
# `card` does not depend on `media`. A card built without it is a legitimate
# card — complete except for the data, and it says exactly which files are
# absent. That is what a build with no network produces.
#
# WHAT `make media` FETCHES, and it is only ever this one file: DevilutionX's
# redistribution of Diablo's 1996 shareware data, `spawn.mpq`. Blizzard
# released the shareware free of charge; the DevilutionX project publishes it
# unmodified as a release asset. It carries the first two dungeon levels and
# the Warrior class.
#
# IT DOES NOT FETCH THE RETAIL GAME. `DIABDAT.MPQ` belongs to Blizzard and is
# sold, not given away. A user who owns Diablo copies their own file into
# media/ by hand — see the README.
#
# Nor does it fetch `devilutionx.mpq`, the game's own fonts and menu artwork.
# That ships inside each official DevilutionX release package, whose name
# changes with the platform and the version, so there is no single stable URL
# to name here and guessing one would break silently. The README says where to
# get it.
#
# The download is plain curl. The result is checked against the SHA256 the
# DevilutionX project publishes, and a provenance file is written beside it.
# Re-running re-verifies rather than re-downloading.
MEDIA_DIR = media

SPAWN_MPQ    = $(MEDIA_DIR)/spawn.mpq
SPAWN_URL    = https://github.com/diasurgical/devilutionx-assets/releases/latest/download/spawn.mpq
SPAWN_SHA256 = 64427cd7c1ba904eaa2e0031c16a6b136d0ecef9abc888c5ff8344b459356e38

# sha256sum on Linux, shasum on macOS. Whichever exists; if neither does, the
# target stops rather than accepting an unverified download.
SHA256SUM := $(firstword $(shell command -v sha256sum 2>/dev/null) \
                         $(shell command -v shasum 2>/dev/null))

media:
	@if [ -z "$(SHA256SUM)" ]; then \
		echo "  MEDIA no sha256sum or shasum on this machine — refusing to"; \
		echo "        download something that cannot be verified."; \
		exit 1; \
	fi
	@mkdir -p $(MEDIA_DIR)
	@if [ -f "$(SPAWN_MPQ)" ]; then \
		echo "  MEDIA $(SPAWN_MPQ) already here — verifying"; \
	else \
		echo "  MEDIA fetching $(SPAWN_URL)"; \
		curl -fL --retry 3 -o "$(SPAWN_MPQ).part" "$(SPAWN_URL)" || { \
			rm -f "$(SPAWN_MPQ).part"; \
			echo "  MEDIA download failed"; exit 1; }; \
		mv "$(SPAWN_MPQ).part" "$(SPAWN_MPQ)"; \
	fi
	@got=`$(SHA256SUM) -a 256 "$(SPAWN_MPQ)" 2>/dev/null || $(SHA256SUM) "$(SPAWN_MPQ)"`; \
	got=`echo "$$got" | awk '{print $$1}'`; \
	if [ "$$got" != "$(SPAWN_SHA256)" ]; then \
		echo "  MEDIA SHA256 MISMATCH for $(SPAWN_MPQ)"; \
		echo "        expected $(SPAWN_SHA256)"; \
		echo "        got      $$got"; \
		echo "        the file has been left in place for inspection, and is"; \
		echo "        NOT safe to put on a card."; \
		exit 1; \
	fi; \
	echo "  MEDIA $(SPAWN_MPQ) verified ($$(wc -c < $(SPAWN_MPQ) | tr -d ' ') bytes)"
	@printf '%s\n' \
		"spawn.mpq — Diablo shareware data" \
		"" \
		"Source:   $(SPAWN_URL)" \
		"Fetched:  `date -u '+%Y-%m-%d %H:%M:%S UTC'`" \
		"SHA256:   $(SPAWN_SHA256)" \
		"" \
		"What it is: the data files of Diablo's 1996 shareware release," \
		"extracted unmodified from Blizzard's own shareware installer and" \
		"republished by the DevilutionX project as a release asset. Blizzard" \
		"distributed the shareware free of charge. It contains the first two" \
		"dungeon levels and the Warrior class only." \
		"" \
		"Diablo is a trademark of Blizzard Entertainment. This file is not" \
		"ours, is not redistributed by this repository, and is downloaded" \
		"only by a person running 'make media' on their own machine." \
		> $(MEDIA_DIR)/provenance.txt
	@echo "  MEDIA provenance written to $(MEDIA_DIR)/provenance.txt"

# The Pi 5 netboot bundle: the image the Pi 5 firmware looks for, plus the
# boot configuration it must be served alongside. Copy the contents into the
# TFTP root the board boots from (the Raspberry Pi firmware files themselves
# come from that root's existing installation, not from here).
NETBOOT_DIR = build/netboot-rpi5
netboot: rpi5
	@mkdir -p $(NETBOOT_DIR)
	@cp host/build/rpi5/$(IMAGE_rpi5) $(NETBOOT_DIR)/
	@cp host/config.txt host/cmdline.txt $(NETBOOT_DIR)/
	@echo "  STAGED $(NETBOOT_DIR)/"
	@ls -l $(NETBOOT_DIR)/

# The card, staged into a directory to copy onto media formatted elsewhere:
# the three kernels, the boot configuration, and whatever game data media/
# happens to hold.
#
# THIS TARGET NEVER DOWNLOADS ANYTHING. It copies what `make media` left and
# names what is absent, so a card built with no network is a real card that
# reports exactly what it is short of.
CARD_DIR  = build/sd-card
CARD_GAME = $(CARD_DIR)/games/devilutionx

# The game's own free artwork, fonts and palettes, which live in the source
# tree rather than in any download: the menu palette alone is what the game
# opens first, and without it there is nothing on screen from the very first
# frame. Everything under it is CC-BY, OFL or zlib.
GAME_ASSETS = devilutionX/Packaging/resources/assets

card: kernels
	@rm -rf $(CARD_DIR)
	@mkdir -p $(CARD_GAME)
	@cp -R $(GAME_ASSETS) $(CARD_GAME)/
	@echo "  ASSETS $(GAME_ASSETS) -> games/devilutionx/assets"
	@cp host/build/rpi3/$(IMAGE_rpi3) $(CARD_DIR)/
	@cp host/build/rpi4/$(IMAGE_rpi4) $(CARD_DIR)/
	@cp host/build/rpi5/$(IMAGE_rpi5) $(CARD_DIR)/
	@cp host/config.txt host/cmdline.txt $(CARD_DIR)/
	@echo "  STAGED $(CARD_DIR)/"
	@for f in spawn.mpq DIABDAT.MPQ diabdat.mpq devilutionx.mpq fonts.mpq \
	          hellfire.mpq hfmonk.mpq hfbard.mpq hfbarb.mpq hfmusic.mpq \
	          hfvoice.mpq; do \
		if [ -f "$(MEDIA_DIR)/$$f" ]; then \
			cp "$(MEDIA_DIR)/$$f" $(CARD_GAME)/; \
			echo "  DATA   $$f"; \
		fi; \
	done
	@echo
	@if [ -f $(CARD_GAME)/devilutionx.mpq ]; then :; else \
		echo "  ABSENT devilutionx.mpq — the game's own fonts and menu art."; \
		echo "         The game cannot draw its menus without it. It ships"; \
		echo "         inside every official DevilutionX release package."; \
	fi
	@if [ -f $(CARD_GAME)/DIABDAT.MPQ ] || [ -f $(CARD_GAME)/diabdat.mpq ] \
	   || [ -f $(CARD_GAME)/spawn.mpq ]; then :; else \
		echo "  ABSENT Diablo's own data. Either DIABDAT.MPQ, copied from a"; \
		echo "         copy of the game you own, or spawn.mpq, the free"; \
		echo "         shareware data — 'make media' fetches that one."; \
	fi
	@echo "  NOTE   The Raspberry Pi firmware files are not staged here either."
	@echo "         See README.md."


# Board build trees and staged output only. media/ is NOT touched: it holds
# things a person downloaded, and a build target does not delete those.
clean-boards:
	@for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b clean-board; done
	rm -rf $(NETBOOT_DIR) $(CARD_DIR)
