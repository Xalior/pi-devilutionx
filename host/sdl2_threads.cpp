//
// sdl2_threads.cpp — SDL's thread and mutex API, on a board that has neither.
//
// THERE ARE NO THREADS HERE. Circle gives each core one line of execution and
// a cooperative scheduler on core 0; this port hands core 1 to the game and
// core 2 to the shim's presentation worker, and that is the whole allocation.
// Nothing can create another.
//
// DevilutionX reaches for SDL threads in two places, and only one of them is
// on a path this port can run:
//
//   nthread.cpp   starts a network thread — but only when `gbIsMultiplayer`,
//                 and this build is compiled NONET, so single player never
//                 asks. `SDL_GetThreadID` is still called on the null thread
//                 to ask "am I the thread that started this?", which is the
//                 one call that must work.
//   appfat.cpp    asks the same question while shutting down after a fatal
//                 error, to avoid recursing into its own cleanup.
//
// So SDL_CreateThread FAILS, honestly and loudly, rather than running the
// handler inline and pretending: a handler that expects to run beside the
// caller and instead runs before it is a deadlock or a corruption, and either
// would be blamed on the game. Upstream's own code turns a null thread into
// `ErrSdl()`, which reports the SDL error text set here.
//
// The mutexes ARE real. Nothing contends for them today, but a lock that
// quietly did nothing would be the wrong thing to leave behind for the day
// something does, so each is a test-and-set over one word — which on an idle
// lock costs a single atomic exchange.
//
#include <cstdlib>

#include <SDL2/SDL.h>

extern "C" {

// ---------------------------------------------------------------------------
// Threads
// ---------------------------------------------------------------------------

// The identifier reported for "the calling thread". Every core in this image
// that runs game code runs one line of execution, and only one of them ever
// reaches the game, so a single constant answers correctly: the game always
// compares this against itself.
static const SDL_threadID THIS_THREAD = 1;

SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data)
{
    (void)fn; (void)data;
    SDL_SetError("cannot start thread \"%s\": this build has no threads — "
                 "the cores are allocated to the game and to presentation",
                 name != nullptr ? name : "(unnamed)");
    return nullptr;
}

void SDL_WaitThread(SDL_Thread *thread, int *status)
{
    // Nothing was ever started, so there is nothing to wait for. A status of
    // zero is what a thread that did nothing would have returned.
    (void)thread;
    if (status != nullptr)
        *status = 0;
}

SDL_threadID SDL_GetThreadID(SDL_Thread *thread)
{
    // A null argument means "the calling thread", which is the only form this
    // port ever sees. A non-null one could only have come from
    // SDL_CreateThread, which never returns one.
    (void)thread;
    return THIS_THREAD;
}

// ---------------------------------------------------------------------------
// Mutexes
// ---------------------------------------------------------------------------

struct SDL_mutex { volatile int held; };

SDL_mutex *SDL_CreateMutex(void)
{
    SDL_mutex *m = (SDL_mutex *)calloc(1, sizeof(SDL_mutex));
    if (m == nullptr)
        SDL_SetError("out of memory allocating mutex");
    return m;
}

void SDL_DestroyMutex(SDL_mutex *mutex)
{
    free(mutex);
}

int SDL_LockMutex(SDL_mutex *mutex)
{
    if (mutex == nullptr)
    {
        SDL_SetError("SDL_LockMutex: no mutex");
        return -1;
    }
    while (__atomic_exchange_n(&mutex->held, 1, __ATOMIC_ACQUIRE) != 0)
        asm volatile("yield" ::: "memory");
    return 0;
}

int SDL_UnlockMutex(SDL_mutex *mutex)
{
    if (mutex == nullptr)
    {
        SDL_SetError("SDL_UnlockMutex: no mutex");
        return -1;
    }
    __atomic_store_n(&mutex->held, 0, __ATOMIC_RELEASE);
    return 0;
}

} // extern "C"
