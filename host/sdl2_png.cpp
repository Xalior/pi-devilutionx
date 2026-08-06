//
// sdl2_png.cpp — the PNG loader DevilutionX declares, reporting honestly that
// this build cannot decode one.
//
// Upstream declares its own small PNG interface in Source/utils/png.h and
// backs it with SDL_image's PNG loader plus libpng. THERE IS EXACTLY ONE
// CALLER of it in a build configured like this one: the on-screen touch
// gamepad's artwork (`Source/controls/touch/renderers.cpp`), which loads
// `ui_art/menu.png` and three siblings out of the assets archive.
//
// That gamepad is only ever drawn when the game's control mode is
// `VirtualGamepad`, which requires a touchscreen. This board has none, so the
// art is loaded at start-up, never drawn, and freed at the end.
//
// Rather than carry libpng, zlib's PNG side and SDL_image's decoder for
// pictures nothing can display, this returns a null surface and sets the SDL
// error. Upstream stores the null and carries on: `LoadArt` puts it in a
// smart pointer, `SDL_CreateTextureFromSurface` is then handed nullptr and
// answers with its own error, and the renderer that would have used it is
// never reached.
//
// THIS IS A SEAM, AND IT IS THE ONE TO OPEN if a touchscreen is ever attached
// to one of these boards, or if anything else in DevilutionX starts loading a
// PNG. Adding libpng as a submodule and writing IMG_LoadPNG_RW over it is
// perhaps a hundred lines; nothing else in this port would change.
//
#include <SDL2/SDL.h>

extern "C" {

SDL_Surface *IMG_LoadPNG_RW(SDL_RWops *src)
{
    (void)src;
    SDL_SetError("PNG decoding is not built into this image: the only PNG "
                 "assets are the on-screen touch gamepad's, and this board "
                 "has no touchscreen");
    return nullptr;
}

} // extern "C"
