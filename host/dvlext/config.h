//
// config.h — the file upstream's CMake build generates, written down instead.
//
// DevilutionX's own build system produces this header at configure time from
// the project name and the VERSION file, and three of its sources include it
// (`msg.cpp`, `DiabloUI/multi/selgame.cpp`, `discord/discord.cpp`). This
// build compiles the game's sources directly, so the header is supplied here
// rather than generated.
//
// THE VERSION MUST MATCH THE PINNED SUBMODULE. It is what the game prints,
// what `--version` reports, and — through msg.cpp — part of the multiplayer
// version check. Advance the devilutionX submodule and these change with it;
// the pinned commit's own VERSION file is the authority.
//
#pragma once

#define PROJECT_NAME "DevilutionX"
#define PROJECT_VERSION "1.5.5"
#define PROJECT_VERSION_MAJOR 1
#define PROJECT_VERSION_MINOR 5
#define PROJECT_VERSION_PATCH 5
