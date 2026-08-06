//
// sdl2_surface.cpp — the SDL2 surface layer DevilutionX draws through.
//
// circle-libsdl2 renders from textures alone: its SDL_Surface is a 32-bit
// staging buffer, and nothing in the library blits, converts, fills or clips
// one. DevilutionX draws the way Diablo has always drawn — into an 8-bit
// paletted buffer, converted to 32 bits once a frame on its way to a
// streaming texture — so that conversion has to exist somewhere. It exists
// here, in this port's own layer, rather than in the game or in the library.
//
// THE FRAME'S PATH, so the pieces below have a shape to sit in:
//
//   PalSurface              an 8-bit INDEX8 surface the whole game draws
//                           into, with the game's palette attached
//   RendererTextureSurface  a 32-bit ARGB8888 surface the size of the
//                           screen
//   texture                 a streaming ARGB8888 texture the library owns
//
// Once a frame DevilutionX blits the first onto the second (SDL_UpperBlit,
// below — the one conversion), hands the second's pixels to SDL_UpdateTexture,
// and asks the library to present. Everything else here serves the menus, the
// cursor and the loading screens.
//
// Two of these functions REPLACE a library function instead of adding one:
// SDL_CreateRGBSurface and SDL_FreeSurface exist in the shim and refuse
// anything but 32 bits. They are reached through the linker's --wrap (see the
// WRAPPED_SDL list in the Makefile), so the library's own versions stay in
// place and still do the 32-bit work — this file only adds the cases they
// refuse and hands everything else straight back. Redefining them outright
// would be a duplicate symbol at best and a silent shadow at worst.
//
// These are seams, not permanent furniture. When the shim implements one of
// these for real, the way to adopt it is to DELETE the version here: the
// archive is linked whole, so a leftover would become a duplicate-symbol
// error at link time rather than a silent winner over the real thing.
//
#include <cstdlib>
#include <cstring>

#include <SDL2/SDL.h>

extern "C" {

// The library's own versions of the two wrapped functions.
SDL_Surface *__real_SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                         int depth, Uint32 Rmask, Uint32 Gmask,
                                         Uint32 Bmask, Uint32 Amask);
void __real_SDL_FreeSurface(SDL_Surface *surface);

// Defined further down; the free path below needs it.
void SDL_FreePalette(SDL_Palette *palette);

// ---------------------------------------------------------------------------
// Surfaces this file owns
// ---------------------------------------------------------------------------
//
// A surface made here carries allocations the library knows nothing about — a
// heap pixel format, sometimes a palette, sometimes the pixels — so freeing it
// is this file's job too. Rather than guess from the surface's contents, every
// one made here is recorded, and the free path looks it up: found means ours
// and freed our way, not found means the library's and handed back to it.

struct OwnedSurface
{
    SDL_Surface     *surface;
    SDL_PixelFormat *format;       // always heap, always ours
    bool             owns_palette; // false when the game attached its own
    bool             owns_pixels;  // false when the caller supplied them
    bool             has_key;      // a colour key was set on this surface
    Uint32           key;
    OwnedSurface    *next;
};

static OwnedSurface *s_owned = nullptr;

static OwnedSurface *FindOwned(const SDL_Surface *surface)
{
    for (OwnedSurface *o = s_owned; o != nullptr; o = o->next)
        if (o->surface == surface)
            return o;
    return nullptr;
}

static SDL_Palette *NewPalette(int ncolors)
{
    SDL_Palette *pal = (SDL_Palette *)calloc(1, sizeof(SDL_Palette));
    if (pal == nullptr)
        return nullptr;

    pal->colors = (SDL_Color *)calloc((size_t)ncolors, sizeof(SDL_Color));
    if (pal->colors == nullptr)
    {
        free(pal);
        return nullptr;
    }
    pal->ncolors  = ncolors;
    pal->refcount = 1;
    // Opaque black until the game sets a real palette. A palette whose alpha
    // was zero throughout would convert to a fully transparent picture and
    // read as "the game drew nothing".
    for (int i = 0; i < ncolors; i++)
        pal->colors[i].a = 0xFF;
    return pal;
}

static void DestroyPalette(SDL_Palette *pal)
{
    if (pal == nullptr)
        return;
    free(pal->colors);
    free(pal);
}

// Fill in a heap pixel format for one of the two layouts this port needs:
// 8-bit indexed, or 32-bit ARGB8888. Anything else is refused rather than
// approximated.
static SDL_PixelFormat *MakeFormat(Uint32 format)
{
    SDL_PixelFormat *fmt = (SDL_PixelFormat *)calloc(1, sizeof(SDL_PixelFormat));
    if (fmt == nullptr)
        return nullptr;

    fmt->format   = format;
    fmt->refcount = 1;

    if (format == SDL_PIXELFORMAT_INDEX8)
    {
        fmt->BitsPerPixel  = 8;
        fmt->BytesPerPixel = 1;
    }
    else
    {
        fmt->BitsPerPixel  = 32;
        fmt->BytesPerPixel = 4;
        fmt->Rmask  = 0x00FF0000;
        fmt->Gmask  = 0x0000FF00;
        fmt->Bmask  = 0x000000FF;
        fmt->Rshift = 16;
        fmt->Gshift = 8;
        fmt->Bshift = 0;
        fmt->Aloss  = 8;
        if (format == SDL_PIXELFORMAT_ARGB8888)
        {
            fmt->Amask = 0xFF000000;
            fmt->Ashift = 24;
            fmt->Aloss = 0;
        }
    }
    return fmt;
}

// The one place a surface is built. pixels == nullptr with prealloc set is
// legitimate: the caller is about to point the surface at memory it locks
// elsewhere.
static SDL_Surface *NewOwnedSurface(int width, int height, Uint32 format,
                                    void *pixels, int pitch)
{
    if (format != SDL_PIXELFORMAT_INDEX8
        && format != SDL_PIXELFORMAT_ARGB8888
        && format != SDL_PIXELFORMAT_RGB888)
    {
        SDL_SetError("surface format 0x%08x is not implemented", (unsigned)format);
        return nullptr;
    }
    if (width <= 0 || height <= 0)
    {
        SDL_SetError("surface dimensions must be positive");
        return nullptr;
    }

    const bool prealloc = (pixels != nullptr);
    const bool indexed  = (format == SDL_PIXELFORMAT_INDEX8);

    SDL_Surface *surface = (SDL_Surface *)calloc(1, sizeof(SDL_Surface));
    OwnedSurface *rec    = (OwnedSurface *)calloc(1, sizeof(OwnedSurface));
    SDL_PixelFormat *fmt = MakeFormat(format);
    SDL_Palette *pal     = indexed ? NewPalette(256) : nullptr;

    if (surface == nullptr || rec == nullptr || fmt == nullptr
        || (indexed && pal == nullptr))
    {
        DestroyPalette(pal);
        free(fmt);
        free(rec);
        free(surface);
        SDL_SetError("out of memory allocating surface");
        return nullptr;
    }

    fmt->palette = pal;

    if (pitch <= 0)
        pitch = width * fmt->BytesPerPixel;

    if (!prealloc)
    {
        pixels = calloc(1, (size_t)pitch * (size_t)height);
        if (pixels == nullptr)
        {
            DestroyPalette(pal);
            free(fmt);
            free(rec);
            free(surface);
            SDL_SetError("out of memory allocating surface pixels");
            return nullptr;
        }
    }

    surface->flags     = prealloc ? SDL_PREALLOC : 0;
    surface->format    = fmt;
    surface->w         = width;
    surface->h         = height;
    surface->pitch     = pitch;
    surface->pixels    = pixels;
    surface->clip_rect = SDL_Rect{ 0, 0, width, height };
    surface->refcount  = 1;

    rec->surface      = surface;
    rec->format       = fmt;
    rec->owns_palette = (pal != nullptr);
    rec->owns_pixels  = !prealloc;
    rec->next         = s_owned;
    s_owned           = rec;

    return surface;
}

// ---------------------------------------------------------------------------
// Creation and destruction
// ---------------------------------------------------------------------------

// The library makes 32-bit surfaces; this adds the paletted ones the game's
// screen buffer needs and leaves everything else to the library.
SDL_Surface *__wrap_SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                         int depth, Uint32 Rmask, Uint32 Gmask,
                                         Uint32 Bmask, Uint32 Amask)
{
    if (depth == 8)
        return NewOwnedSurface(width, height, SDL_PIXELFORMAT_INDEX8, nullptr, 0);

    return __real_SDL_CreateRGBSurface(flags, width, height, depth,
                                       Rmask, Gmask, Bmask, Amask);
}

void __wrap_SDL_FreeSurface(SDL_Surface *surface)
{
    if (surface == nullptr)
        return;

    OwnedSurface **link = &s_owned;
    for (OwnedSurface *o = s_owned; o != nullptr; link = &o->next, o = o->next)
    {
        if (o->surface != surface)
            continue;

        if (--surface->refcount > 0)
            return;

        *link = o->next;
        // A palette this file made is destroyed with the surface. One the
        // game attached is shared, so the surface only gives up its own
        // reference to it — exactly as SDL2 does — and the game's own
        // pointer keeps it alive until the game lets go.
        if (o->owns_palette)
            DestroyPalette(o->format->palette);
        else if (o->format->palette != nullptr)
            SDL_FreePalette(o->format->palette);
        if (o->owns_pixels)
            free(surface->pixels);
        free(o->format);
        free(o);
        free(surface);
        return;
    }

    __real_SDL_FreeSurface(surface);
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height,
                                            int depth, Uint32 format)
{
    (void)flags; (void)depth;
    return NewOwnedSurface(width, height, format, nullptr, 0);
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int width,
                                                int height, int depth,
                                                int pitch, Uint32 format)
{
    (void)depth;
    return NewOwnedSurface(width, height, format, pixels, pitch);
}

// ---------------------------------------------------------------------------
// Palettes
// ---------------------------------------------------------------------------
//
// DevilutionX keeps one palette of its own and attaches it to the 8-bit
// screen surface, then changes its colours as the game dims, flashes and
// fades. The surface does not own that palette, so attaching it takes the
// ownership flag off — otherwise freeing the surface would take the game's
// live palette with it.

SDL_Palette *SDL_AllocPalette(int ncolors)
{
    SDL_Palette *pal = NewPalette(ncolors);
    if (pal == nullptr)
        SDL_SetError("out of memory allocating palette");
    return pal;
}

void SDL_FreePalette(SDL_Palette *palette)
{
    if (palette == nullptr)
        return;
    if (--palette->refcount > 0)
        return;
    DestroyPalette(palette);
}

int SDL_SetSurfacePalette(SDL_Surface *surface, SDL_Palette *palette)
{
    if (surface == nullptr || surface->format == nullptr)
    {
        SDL_SetError("SDL_SetSurfacePalette: no surface");
        return -1;
    }
    if (surface->format->BitsPerPixel != 8)
    {
        SDL_SetError("SDL_SetSurfacePalette: surface is not paletted");
        return -1;
    }

    OwnedSurface *o = FindOwned(surface);
    if (o != nullptr && o->owns_palette)
    {
        DestroyPalette(surface->format->palette);
        o->owns_palette = false;
    }
    surface->format->palette = palette;
    if (palette != nullptr)
        palette->refcount++;
    return 0;
}

int SDL_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors,
                         int firstcolor, int ncolors)
{
    if (palette == nullptr || colors == nullptr)
    {
        SDL_SetError("SDL_SetPaletteColors: no palette");
        return -1;
    }
    if (firstcolor < 0 || ncolors < 0 || firstcolor + ncolors > palette->ncolors)
    {
        SDL_SetError("SDL_SetPaletteColors: range outside the palette");
        return -1;
    }

    for (int i = 0; i < ncolors; i++)
    {
        SDL_Color c = colors[i];
        // The game fills only r, g and b. A zero alpha here would convert the
        // whole picture to transparent.
        c.a = 0xFF;
        palette->colors[firstcolor + i] = c;
    }
    palette->version++;
    return 0;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b)
{
    if (format == nullptr)
        return 0;

    if (format->palette != nullptr)
    {
        // Nearest colour by squared distance, which is what SDL does.
        int best = 0;
        int bestDistance = 1 << 30;
        for (int i = 0; i < format->palette->ncolors; i++)
        {
            const SDL_Color &c = format->palette->colors[i];
            const int dr = (int)c.r - (int)r;
            const int dg = (int)c.g - (int)g;
            const int db = (int)c.b - (int)b;
            const int d  = dr * dr + dg * dg + db * db;
            if (d < bestDistance)
            {
                bestDistance = d;
                best = i;
                if (d == 0)
                    break;
            }
        }
        return (Uint32)best;
    }

    return format->Amask
           | ((Uint32)r << format->Rshift)
           | ((Uint32)g << format->Gshift)
           | ((Uint32)b << format->Bshift);
}

// ---------------------------------------------------------------------------
// Colour keys and clipping
// ---------------------------------------------------------------------------

// The colour key lives in this file's own record for the surface, not in the
// surface itself: SDL2 keeps it inside a private blit map that does not exist
// here, and there is no spare field in SDL_Surface to borrow that some other
// part of the game might also want. Every surface DevilutionX creates comes
// through this file, so every surface it can key is one this file records.
int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key)
{
    if (surface == nullptr)
    {
        SDL_SetError("SDL_SetColorKey: no surface");
        return -1;
    }

    OwnedSurface *o = FindOwned(surface);
    if (o == nullptr)
    {
        SDL_SetError("SDL_SetColorKey: this surface came from the library and "
                     "cannot carry a colour key");
        return -1;
    }

    o->has_key = (flag != 0);
    o->key     = key;
    return 0;
}

// Whether this surface has a colour key, and what it is. A key of 0 is
// meaningful — it is what the mouse cursor uses — so the flag is separate
// from the value.
static bool ColorKeyOf(const SDL_Surface *surface, Uint32 *key)
{
    const OwnedSurface *o = FindOwned(surface);
    if (o == nullptr || !o->has_key)
        return false;
    *key = o->key;
    return true;
}

SDL_bool SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect)
{
    if (surface == nullptr)
        return SDL_FALSE;

    SDL_Rect full = { 0, 0, surface->w, surface->h };
    if (rect == nullptr)
    {
        surface->clip_rect = full;
        return SDL_TRUE;
    }

    SDL_Rect r = *rect;
    if (r.x < 0) { r.w += r.x; r.x = 0; }
    if (r.y < 0) { r.h += r.y; r.y = 0; }
    if (r.x + r.w > surface->w) r.w = surface->w - r.x;
    if (r.y + r.h > surface->h) r.h = surface->h - r.y;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;

    surface->clip_rect = r;
    return (r.w > 0 && r.h > 0) ? SDL_TRUE : SDL_FALSE;
}

// ---------------------------------------------------------------------------
// Filling and blitting
// ---------------------------------------------------------------------------

int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color)
{
    if (dst == nullptr || dst->pixels == nullptr)
    {
        SDL_SetError("SDL_FillRect: no destination");
        return -1;
    }

    SDL_Rect r = (rect != nullptr) ? *rect : dst->clip_rect;

    // Clipped against the surface's clip rectangle, which is what SDL does.
    const SDL_Rect &c = dst->clip_rect;
    int x0 = r.x > c.x ? r.x : c.x;
    int y0 = r.y > c.y ? r.y : c.y;
    int x1 = (r.x + r.w) < (c.x + c.w) ? (r.x + r.w) : (c.x + c.w);
    int y1 = (r.y + r.h) < (c.y + c.h) ? (r.y + r.h) : (c.y + c.h);
    if (x1 <= x0 || y1 <= y0)
        return 0;

    const int bpp = dst->format->BytesPerPixel;
    for (int y = y0; y < y1; y++)
    {
        Uint8 *row = (Uint8 *)dst->pixels + (size_t)y * dst->pitch
                     + (size_t)x0 * bpp;
        if (bpp == 1)
        {
            memset(row, (int)(color & 0xFF), (size_t)(x1 - x0));
        }
        else
        {
            Uint32 *p = (Uint32 *)row;
            for (int x = 0; x < x1 - x0; x++)
                p[x] = color;
        }
    }
    return 0;
}

// One flat lookup table per blit, built from the palette as it stands. The
// game changes its palette on damage, on spells and in the menus, so it is
// rebuilt every blit rather than cached.
static void BuildPaletteLUT(const SDL_Palette *pal, Uint32 lut[256])
{
    const int n = (pal != nullptr && pal->ncolors < 256) ? pal->ncolors : 256;
    for (int i = 0; i < n; i++)
    {
        const SDL_Color &c = pal->colors[i];
        lut[i] = 0xFF000000u | ((Uint32)c.r << 16) | ((Uint32)c.g << 8)
                 | (Uint32)c.b;
    }
    for (int i = n; i < 256; i++)
        lut[i] = 0xFF000000u;
}

// The frame's one conversion, and the workhorse of this file: the game's
// 8-bit screen buffer through its palette into 32-bit ARGB, or a same-format
// copy. Both honour the destination's clip rectangle and the source's colour
// key, which is how the cursor and the menu art keep their transparency.
int SDL_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect,
                  SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (src == nullptr || dst == nullptr
        || src->pixels == nullptr || dst->pixels == nullptr)
    {
        SDL_SetError("SDL_UpperBlit: no source or no destination");
        return -1;
    }

    SDL_Rect sr = (srcrect != nullptr) ? *srcrect : SDL_Rect{ 0, 0, src->w, src->h };
    SDL_Rect dr = (dstrect != nullptr) ? *dstrect : SDL_Rect{ 0, 0, src->w, src->h };
    dr.w = sr.w;
    dr.h = sr.h;

    // Clip the source against its own memory first, moving the destination
    // with it so the two stay aligned.
    if (sr.x < 0) { dr.x -= sr.x; sr.w += sr.x; sr.x = 0; }
    if (sr.y < 0) { dr.y -= sr.y; sr.h += sr.y; sr.y = 0; }
    if (sr.x + sr.w > src->w) sr.w = src->w - sr.x;
    if (sr.y + sr.h > src->h) sr.h = src->h - sr.y;

    // Then against the destination's clip rectangle, again moving the source.
    const SDL_Rect &c = dst->clip_rect;
    if (dr.x < c.x) { const int d = c.x - dr.x; sr.x += d; sr.w -= d; dr.x = c.x; }
    if (dr.y < c.y) { const int d = c.y - dr.y; sr.y += d; sr.h -= d; dr.y = c.y; }
    if (dr.x + sr.w > c.x + c.w) sr.w = c.x + c.w - dr.x;
    if (dr.y + sr.h > c.y + c.h) sr.h = c.y + c.h - dr.y;

    const int w = sr.w;
    const int h = sr.h;
    dr.w = w;
    dr.h = h;
    if (dstrect != nullptr)
        *dstrect = dr;
    if (w <= 0 || h <= 0)
        return 0;

    const int sbpp = src->format->BytesPerPixel;
    const int dbpp = dst->format->BytesPerPixel;

    Uint32 key = 0;
    const bool keyed = ColorKeyOf(src, &key);

    if (sbpp == 1 && dbpp == 4)
    {
        Uint32 lut[256];
        BuildPaletteLUT(src->format->palette, lut);

        for (int y = 0; y < h; y++)
        {
            const Uint8 *s = (const Uint8 *)src->pixels
                             + (size_t)(sr.y + y) * src->pitch + sr.x;
            Uint32 *d = (Uint32 *)((Uint8 *)dst->pixels
                        + (size_t)(dr.y + y) * dst->pitch) + dr.x;
            if (keyed)
            {
                for (int x = 0; x < w; x++)
                    if (s[x] != (Uint8)key)
                        d[x] = lut[s[x]];
            }
            else
            {
                for (int x = 0; x < w; x++)
                    d[x] = lut[s[x]];
            }
        }
        return 0;
    }

    if (sbpp == dbpp)
    {
        for (int y = 0; y < h; y++)
        {
            const Uint8 *s = (const Uint8 *)src->pixels
                             + (size_t)(sr.y + y) * src->pitch
                             + (size_t)sr.x * sbpp;
            Uint8 *d = (Uint8 *)dst->pixels + (size_t)(dr.y + y) * dst->pitch
                       + (size_t)dr.x * dbpp;
            if (!keyed)
            {
                memcpy(d, s, (size_t)w * sbpp);
            }
            else if (sbpp == 1)
            {
                for (int x = 0; x < w; x++)
                    if (s[x] != (Uint8)key)
                        d[x] = s[x];
            }
            else
            {
                const Uint32 *s32 = (const Uint32 *)s;
                Uint32 *d32 = (Uint32 *)d;
                for (int x = 0; x < w; x++)
                    if ((s32[x] & 0x00FFFFFFu) != (key & 0x00FFFFFFu))
                        d32[x] = s32[x];
            }
        }
        return 0;
    }

    SDL_SetError("SDL_UpperBlit: %d-bit to %d-bit is not implemented",
                 sbpp * 8, dbpp * 8);
    return -1;
}

// Nearest-neighbour scaling. It exists for the hardware cursor, which asks
// for its bitmap at the display's scale, and for the on-screen touch gamepad
// art. Neither is on a frame's critical path, so plainness beats cleverness.
int SDL_UpperBlitScaled(SDL_Surface *src, const SDL_Rect *srcrect,
                        SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (src == nullptr || dst == nullptr
        || src->pixels == nullptr || dst->pixels == nullptr)
    {
        SDL_SetError("SDL_UpperBlitScaled: no source or no destination");
        return -1;
    }

    const SDL_Rect sr = (srcrect != nullptr)
                        ? *srcrect : SDL_Rect{ 0, 0, src->w, src->h };
    SDL_Rect dr = (dstrect != nullptr)
                  ? *dstrect : SDL_Rect{ 0, 0, dst->w, dst->h };

    if (sr.w <= 0 || sr.h <= 0 || dr.w <= 0 || dr.h <= 0)
        return 0;

    const int sbpp = src->format->BytesPerPixel;
    const int dbpp = dst->format->BytesPerPixel;
    if (!((sbpp == 1 && dbpp == 4) || sbpp == dbpp))
    {
        SDL_SetError("SDL_UpperBlitScaled: %d-bit to %d-bit is not implemented",
                     sbpp * 8, dbpp * 8);
        return -1;
    }

    Uint32 lut[256];
    if (sbpp == 1 && dbpp == 4)
        BuildPaletteLUT(src->format->palette, lut);

    Uint32 key = 0;
    const bool keyed = ColorKeyOf(src, &key);

    const SDL_Rect &c = dst->clip_rect;
    for (int y = 0; y < dr.h; y++)
    {
        const int dy = dr.y + y;
        if (dy < c.y || dy >= c.y + c.h)
            continue;
        const int sy = sr.y + (int)((long long)y * sr.h / dr.h);
        const Uint8 *srow = (const Uint8 *)src->pixels + (size_t)sy * src->pitch;
        Uint8 *drow = (Uint8 *)dst->pixels + (size_t)dy * dst->pitch;

        for (int x = 0; x < dr.w; x++)
        {
            const int dx = dr.x + x;
            if (dx < c.x || dx >= c.x + c.w)
                continue;
            const int sx = sr.x + (int)((long long)x * sr.w / dr.w);

            if (sbpp == 1)
            {
                const Uint8 v = srow[sx];
                if (keyed && v == (Uint8)key)
                    continue;
                if (dbpp == 4)
                    ((Uint32 *)drow)[dx] = lut[v];
                else
                    drow[dx] = v;
            }
            else
            {
                const Uint32 v = ((const Uint32 *)srow)[sx];
                if (keyed && (v & 0x00FFFFFFu) == (key & 0x00FFFFFFu))
                    continue;
                ((Uint32 *)drow)[dx] = v;
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Format conversion
// ---------------------------------------------------------------------------

SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 pixel_format,
                                      Uint32 flags)
{
    (void)flags;
    if (src == nullptr)
    {
        SDL_SetError("SDL_ConvertSurfaceFormat: no source");
        return nullptr;
    }

    SDL_Surface *out = NewOwnedSurface(src->w, src->h, pixel_format, nullptr, 0);
    if (out == nullptr)
        return nullptr;

    // A conversion copies every pixel, colour key and all: the key travels
    // with the surface rather than punching holes in the copy. So it is
    // lifted off for the duration of the copy and put back afterwards.
    OwnedSurface *rec = FindOwned(src);
    const bool savedKey = (rec != nullptr) && rec->has_key;
    if (rec != nullptr)
        rec->has_key = false;
    const int rc = SDL_UpperBlit(src, nullptr, out, nullptr);
    if (rec != nullptr)
        rec->has_key = savedKey;

    if (rc != 0)
    {
        __wrap_SDL_FreeSurface(out);
        return nullptr;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Surface to texture
// ---------------------------------------------------------------------------
//
// The library's textures are write-only from the application's side: they are
// created, locked, filled and presented. So this converts through a lock
// rather than asking the library for anything it does not have.

SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *renderer,
                                          SDL_Surface *surface)
{
    if (renderer == nullptr || surface == nullptr || surface->pixels == nullptr)
    {
        SDL_SetError("SDL_CreateTextureFromSurface: no surface");
        return nullptr;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             surface->w, surface->h);
    if (texture == nullptr)
        return nullptr;

    void *pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0)
    {
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    SDL_Surface staging{};
    SDL_PixelFormat fmt{};
    fmt.format        = SDL_PIXELFORMAT_ARGB8888;
    fmt.BitsPerPixel  = 32;
    fmt.BytesPerPixel = 4;
    fmt.Rmask = 0x00FF0000; fmt.Gmask = 0x0000FF00;
    fmt.Bmask = 0x000000FF; fmt.Amask = 0xFF000000;
    fmt.Rshift = 16; fmt.Gshift = 8; fmt.Bshift = 0; fmt.Ashift = 24;
    staging.format    = &fmt;
    staging.w         = surface->w;
    staging.h         = surface->h;
    staging.pitch     = pitch;
    staging.pixels    = pixels;
    staging.clip_rect = SDL_Rect{ 0, 0, surface->w, surface->h };

    const int rc = SDL_UpperBlit(surface, nullptr, &staging, nullptr);
    SDL_UnlockTexture(texture);

    if (rc != 0)
    {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    return texture;
}

} // extern "C"
