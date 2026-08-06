//
// circle_stubs.cpp — the window, renderer, logging and odds-and-ends entry
// points DevilutionX references that circle-libsdl2 does not implement.
//
// Each one either does the job properly or fails honestly — returns an error,
// returns null — so that nothing pretends to work. Where a function is a
// deliberate no-op it says why: on a bare-metal board with one fullscreen
// display, no window manager and no clipboard, several of SDL's window calls
// have nothing left to do.
//
// LOGGING GOES THROUGH THE SHIM, NEVER THROUGH CIRCLE'S LOGGER. A device
// belongs to core 0, the serial console is a device, and the game runs on the
// application core — so every SDL_Log* below hands its line to
// SDL2Circle_Log, which puts it in the calling core's ring for core 0's servo
// to drain, and writes straight through when the caller is core 0 already.
//
// These are seams, not permanent furniture. When the shim implements one of
// these for real, the way to adopt it is to DELETE the version here: the
// archive is linked whole, so a leftover would become a duplicate-symbol
// error at link time rather than a silent winner over the real thing.
//
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_circle.h>

extern "C" {

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
//
// SDL's log API is a priority per category on top of one output function.
// DevilutionX uses the application category for everything it writes itself
// and takes SDL's own default priorities for the rest, so the table below is
// the whole of the state.

static SDL_LogPriority s_priorities[SDL_LOG_CATEGORY_CUSTOM + 1];
static bool s_prioritiesReady = false;

static void EnsurePriorities(void)
{
    if (s_prioritiesReady)
        return;
    // SDL2's defaults: everything at INFO, the application category at INFO
    // as well, assertions and tests noisier. Matching them means a build that
    // sets nothing behaves the way the game's authors expect.
    for (unsigned i = 0; i <= SDL_LOG_CATEGORY_CUSTOM; i++)
        s_priorities[i] = SDL_LOG_PRIORITY_INFO;
    s_priorities[SDL_LOG_CATEGORY_ASSERT] = SDL_LOG_PRIORITY_WARN;
    s_priorities[SDL_LOG_CATEGORY_TEST]   = SDL_LOG_PRIORITY_VERBOSE;
    s_prioritiesReady = true;
}

static SDL_LogPriority PriorityOf(int category)
{
    EnsurePriorities();
    if (category < 0 || category > (int)SDL_LOG_CATEGORY_CUSTOM)
        category = SDL_LOG_CATEGORY_CUSTOM;
    return s_priorities[category];
}

// SDL's six priorities onto the shim's four severities. VERBOSE and DEBUG
// have no separate home, so they arrive as notices — which is the honest
// mapping: the shim's ring carries a severity, not a level.
static unsigned ShimSeverity(SDL_LogPriority priority)
{
    switch (priority)
    {
    case SDL_LOG_PRIORITY_CRITICAL:
    case SDL_LOG_PRIORITY_ERROR:    return SDL2CIRCLE_LOG_ERROR;
    case SDL_LOG_PRIORITY_WARN:     return SDL2CIRCLE_LOG_WARNING;
    default:                        return SDL2CIRCLE_LOG_NOTICE;
    }
}

void SDL_LogMessageV(int category, SDL_LogPriority priority,
                     const char *fmt, va_list ap)
{
    if (priority < PriorityOf(category))
        return;

    // The shim's log entry point is not itself variadic-forwarding, so the
    // line is formatted here and handed over whole. The buffer is on the
    // caller's stack, which is the calling core's own.
    char line[1024];
    vsnprintf(line, sizeof(line), fmt, ap);
    SDL2Circle_Log("devilutionx", ShimSeverity(priority), "%s", line);
}

void SDL_LogMessage(int category, SDL_LogPriority priority, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    SDL_LogMessageV(category, priority, fmt, ap);
    va_end(ap);
}

#define SDL_LOG_AT(name, priority)                                          \
    void name(int category, const char *fmt, ...)                           \
    {                                                                       \
        va_list ap;                                                         \
        va_start(ap, fmt);                                                  \
        SDL_LogMessageV(category, priority, fmt, ap);                       \
        va_end(ap);                                                         \
    }

SDL_LOG_AT(SDL_LogVerbose,  SDL_LOG_PRIORITY_VERBOSE)
SDL_LOG_AT(SDL_LogDebug,    SDL_LOG_PRIORITY_DEBUG)
SDL_LOG_AT(SDL_LogInfo,     SDL_LOG_PRIORITY_INFO)
SDL_LOG_AT(SDL_LogWarn,     SDL_LOG_PRIORITY_WARN)
SDL_LOG_AT(SDL_LogError,    SDL_LOG_PRIORITY_ERROR)
SDL_LOG_AT(SDL_LogCritical, SDL_LOG_PRIORITY_CRITICAL)

#undef SDL_LOG_AT

// SDL_Log is the application category at INFO, by SDL's own definition.
void SDL_Log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
    va_end(ap);
}

void SDL_LogSetPriority(int category, SDL_LogPriority priority)
{
    EnsurePriorities();
    if (category >= 0 && category <= (int)SDL_LOG_CATEGORY_CUSTOM)
        s_priorities[category] = priority;
}

void SDL_LogSetAllPriority(SDL_LogPriority priority)
{
    EnsurePriorities();
    for (unsigned i = 0; i <= SDL_LOG_CATEGORY_CUSTOM; i++)
        s_priorities[i] = priority;
}

SDL_LogPriority SDL_LogGetPriority(int category)
{
    return PriorityOf(category);
}

// ---------------------------------------------------------------------------
// Where the game's files live
// ---------------------------------------------------------------------------
//
// On a desktop these are the program's directory and a per-user directory.
// Here both are this game's own directory on the card, which is also where
// its MPQ archives are. The caller frees what it gets, so each hands back a
// fresh copy.

static char *DupGameDir(void)
{
    static const char path[] = RAPI_GAME_DIR "/";
    char *copy = (char *)SDL_malloc(sizeof(path));
    if (copy != nullptr)
        memcpy(copy, path, sizeof(path));
    else
        SDL_SetError("out of memory answering with the game directory");
    return copy;
}

char *SDL_GetBasePath(void) { return DupGameDir(); }
char *SDL_GetPrefPath(const char *, const char *) { return DupGameDir(); }

// ---------------------------------------------------------------------------
// Window calls with nothing to do on this board
// ---------------------------------------------------------------------------
//
// The display is one fullscreen panel that the host kernel declared before the
// game started. Its size, its position, its stacking order and its input grab
// are all settled before any of these can be called, so each answers success
// and changes nothing — which is the truth, not a pretence.

int  SDL_SetWindowFullscreen(SDL_Window *, Uint32) { return 0; }
void SDL_SetWindowSize(SDL_Window *, int, int) {}
void SDL_SetWindowResizable(SDL_Window *, SDL_bool) {}
void SDL_SetWindowGrab(SDL_Window *, SDL_bool) {}
void SDL_HideWindow(SDL_Window *) {}
void SDL_RaiseWindow(SDL_Window *) {}
void SDL_RestoreWindow(SDL_Window *) {}
void SDL_DisableScreenSaver(void) {}

// There is one window and it always has the keyboard, because there is
// nowhere else for a key to go. The shim already answers the same question
// about the pointer, and its answer names the same single window.
SDL_Window *SDL_GetKeyboardFocus(void)
{
    return SDL_GetMouseFocus();
}

// The mode the game is being shown at IS the virtual display the host kernel
// declared, which is what the shim reports as display 0's current mode. There
// is no second mode to switch between, so setting one succeeds and changes
// nothing.
int SDL_GetWindowDisplayMode(SDL_Window *, SDL_DisplayMode *mode)
{
    if (mode == nullptr)
    {
        SDL_SetError("SDL_GetWindowDisplayMode: no mode to fill in");
        return -1;
    }
    return SDL_GetCurrentDisplayMode(0, mode);
}

int SDL_SetWindowDisplayMode(SDL_Window *, const SDL_DisplayMode *) { return 0; }

Uint32 SDL_GetWindowPixelFormat(SDL_Window *)
{
    // Every pixel in this image is 32-bit ARGB8888: the framebuffer, the
    // library's textures, and this port's own surfaces.
    return SDL_PIXELFORMAT_ARGB8888;
}

// A physical panel over HDMI reports no size in millimetres that anything
// here can read, so the dots-per-inch is genuinely unknown. SDL's own
// contract for that is failure, and the one caller — the touch gamepad's
// button sizing — has a fallback for exactly this answer.
int SDL_GetDisplayDPI(int, float *, float *, float *)
{
    SDL_SetError("the physical display's DPI is not known on this board");
    return -1;
}

// The window-surface path. DevilutionX uses it only when its `upscale`
// setting is off, in which case it draws straight into the window's own
// surface instead of into a texture. circle-libsdl2 has no window surface —
// it presents from textures alone — so this fails, and the failure is
// upstream's own `ErrSdl()` with this message on it.
//
// The card's settings file therefore has to leave `Upscale` on, which is
// upstream's default. See the README.
SDL_Surface *SDL_GetWindowSurface(SDL_Window *)
{
    SDL_SetError("this port has no window surface: the renderer path is the "
                 "only one implemented, so the Upscale setting must stay on");
    return nullptr;
}

int SDL_UpdateWindowSurface(SDL_Window *)
{
    SDL_SetError("this port has no window surface to update");
    return -1;
}

// ---------------------------------------------------------------------------
// Renderer geometry
// ---------------------------------------------------------------------------
//
// The library scales the finished frame onto the panel itself, on the
// presentation core, and the window it reports to the game IS the virtual
// display. So from the game's side output coordinates and logical
// coordinates are the same coordinates, and these three report exactly that:
// a viewport covering the whole window, a scale of one, and a logical size
// that is already what was asked for.
//
// This is what keeps the mouse in the right place. DevilutionX maps pointer
// positions through SDL_RenderGetViewport and SDL_RenderGetScale; the shim
// already clamps the pointer to the window, so the identity is the correct
// transform and any other answer would move the cursor away from the hand.

void SDL_RenderGetViewport(SDL_Renderer *renderer, SDL_Rect *rect)
{
    if (rect == nullptr)
        return;
    rect->x = 0;
    rect->y = 0;
    if (SDL_GetRendererOutputSize(renderer, &rect->w, &rect->h) != 0)
    {
        rect->w = 0;
        rect->h = 0;
    }
}

void SDL_RenderGetScale(SDL_Renderer *, float *scaleX, float *scaleY)
{
    if (scaleX != nullptr) *scaleX = 1.0f;
    if (scaleY != nullptr) *scaleY = 1.0f;
}

int SDL_RenderSetLogicalSize(SDL_Renderer *, int, int) { return 0; }
int SDL_RenderSetIntegerScale(SDL_Renderer *, SDL_bool) { return 0; }

// ---------------------------------------------------------------------------
// The clipboard
// ---------------------------------------------------------------------------
//
// There is no clipboard on this board and nothing to share one with. Saying
// so is better than an empty one that silently swallows a copy: the game's
// chat log offers a copy command, and a user who sees it succeed would expect
// to paste it somewhere.

SDL_bool SDL_HasClipboardText(void) { return SDL_FALSE; }

char *SDL_GetClipboardText(void)
{
    // SDL's contract is a freeable empty string rather than null when there
    // is nothing to hand back.
    char *empty = (char *)SDL_malloc(1);
    if (empty != nullptr)
        empty[0] = '\0';
    return empty;
}

int SDL_SetClipboardText(const char *)
{
    SDL_SetError("there is no clipboard on this board");
    return -1;
}

// ---------------------------------------------------------------------------
// Odds and ends
// ---------------------------------------------------------------------------

// Custom event types. DevilutionX registers a block of its own at start-up
// and compares incoming event types against it, so the numbers only have to
// be unique and above SDL's own.
Uint32 SDL_RegisterEvents(int numevents)
{
    static Uint32 s_next = SDL_USEREVENT;
    if (numevents <= 0 || (Uint32)numevents > SDL_LASTEVENT - s_next)
        return (Uint32)-1;
    const Uint32 base = s_next;
    s_next += (Uint32)numevents;
    return base;
}

// Where an on-screen keyboard would put itself. There is none.
void SDL_SetTextInputRect(const SDL_Rect *) {}

// The board has no window manager to put a dialog on top of, so the message
// goes where every other diagnostic goes: the serial console, through the
// shim's log so that it is legal from the game's core.
int SDL_ShowSimpleMessageBox(Uint32, const char *title, const char *message,
                             SDL_Window *)
{
    SDL2Circle_Log("devilutionx", SDL2CIRCLE_LOG_ERROR, "%s: %s",
                   title != nullptr ? title : "message",
                   message != nullptr ? message : "");
    return 0;
}

int SDL_atoi(const char *str)
{
    return atoi(str);
}

// ---------------------------------------------------------------------------
// One POSIX call newlib does not carry
// ---------------------------------------------------------------------------
//
// DevilutionX asks whether a file exists, and whether it can be written to,
// with access(). Circle's newlib has the declaration but no implementation.
// stat() answers the first question exactly; for the second, this filesystem
// is FAT with no permission bits at all, so anything that exists and is not a
// directory is writable — which is the truth about the medium, not an
// assumption about the file.
int access(const char *path, int mode)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;

    if ((mode & W_OK) != 0 && S_ISDIR(st.st_mode))
    {
        errno = EACCES;
        return -1;
    }
    if ((mode & X_OK) != 0 && !S_ISDIR(st.st_mode))
    {
        errno = EACCES;
        return -1;
    }
    return 0;
}

} // extern "C"
