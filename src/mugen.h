/*
 * Common header of the engine sources. Include it first.
 */
#ifndef MUGEN_H
#define MUGEN_H

#include "platform/platform.h"   /* Win32 / POSIX headers and compiler spellings (phase P2) */

#include <SDL.h>
#include <unzip.h>
#include <png.h>

#include "crt.h"
#include "ftol.h"

/* Version reported by the original build. */
#define MUGEN_VERSION_MAJOR 1
#define MUGEN_VERSION_MINOR 1
#define MUGEN_VERSION_PATCH 0
#define MUGEN_VERSION_TAG   "Beta 1 P1"
#define MUGEN_BUILD_YEAR    2013
#define MUGEN_BUILD_MONTH   8
#define MUGEN_BUILD_DAY     11

/*
 * Checks that a decompiled struct matches the layout of the original. The sizes are those of the 32-bit
 * x86 binary (4-byte pointers), so they only hold in the Windows reference build; on Android (arm64, 8-byte
 * pointers, decision D-001) the layout differs and the checks compile to nothing. Behaviour, not memory
 * layout, is what the differential tests compare.
 */
#if defined(MUGEN_PLATFORM_WIN32)
#define MUGEN_ASSERT_SIZE(type, size) \
    _Static_assert(sizeof(type) == (size), "sizeof(" #type ") != " #size)
#define MUGEN_ASSERT_OFFSET(type, field, offset) \
    _Static_assert(offsetof(type, field) == (offset), "offsetof(" #type ", " #field ") != " #offset)
#else
#define MUGEN_ASSERT_SIZE(type, size) _Static_assert(1, "layout check of " #type " skipped: not the 32-bit build")
#define MUGEN_ASSERT_OFFSET(type, field, offset) \
    _Static_assert(1, "offset check of " #type "." #field " skipped: not the 32-bit build")
#endif

#include "util.h"
#include "stream.h"
#include "ini.h"
#include "palette.h"
#include "keyedarray.h"
#include "hashtable.h"
#include "luautil.h"
#include "ai.h"
#include "sprite.h"
#include "sff.h"
#include "textring.h"
#include "log.h"
#include "gfx.h"
#include "glrender.h"
#include "video.h"
#include "events.h"
#include "sound.h"
#include "bgm.h"
#include "keybind.h"
#include "font.h"
#include "syselem.h"
#include "fight.h"
#include "clsn.h"
#include "anim.h"
#include "bg.h"
#include "stage.h"
#include "expr.h"
#include "trigger.h"
#include "select.h"
#include "title.h"
#include "motif.h"
#include "storyboard.h"
#include "game.h"
#include "console.h"
#include "char.h"
#include "player.h"
#include "cnsparse.h"
#include "statedef.h"
#include "playerstate.h"
#include "sctrl.h"
#include "result.h"
#include "pngio.h"
#include "spritecodec.h"
#include "model.h"

#include "symbols.h"

#endif /* MUGEN_H */
