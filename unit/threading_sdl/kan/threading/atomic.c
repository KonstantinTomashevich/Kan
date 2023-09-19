#include <SDL_atomic.h>

#include <kan/threading/atomic.h>

struct kan_atomic_int_t kan_atomic_int_init (int value)
{
    struct kan_atomic_int_t atomic;
    atomic.value = value;
    return atomic;
}

_Static_assert (sizeof (struct kan_atomic_int_t) == sizeof (SDL_SpinLock),
                "Check that spin lock and Kan atomic are the same.");

void kan_atomic_int_lock (struct kan_atomic_int_t *atomic)
{
    SDL_AtomicLock ((SDL_SpinLock *) atomic);
}

kan_bool_t kan_atomic_int_try_lock (struct kan_atomic_int_t *atomic)
{
    // It is SDL_bool, so it correctly converts to our types.
    return SDL_AtomicTryLock ((SDL_SpinLock *) atomic) == SDL_TRUE ? KAN_TRUE : KAN_FALSE;
}

void kan_atomic_int_unlock (struct kan_atomic_int_t *atomic)
{
    SDL_AtomicUnlock ((SDL_SpinLock *) atomic);
}

_Static_assert (sizeof (struct kan_atomic_int_t) == sizeof (SDL_AtomicInt),
                "Check that SDL atomic and Kan atomic are the same.");

int kan_atomic_int_add (struct kan_atomic_int_t *atomic, int delta)
{
    return SDL_AtomicAdd ((SDL_AtomicInt *) atomic, delta);
}

int kan_atomic_int_set (struct kan_atomic_int_t *atomic, int new_value)
{
    return SDL_AtomicSet ((SDL_AtomicInt *) atomic, new_value);
}

kan_bool_t kan_atomic_int_compare_and_set (struct kan_atomic_int_t *atomic, int old_value, int new_value)
{
    return SDL_AtomicCAS ((SDL_AtomicInt *) atomic, old_value, new_value) == SDL_TRUE ? KAN_TRUE : KAN_FALSE;
}

int kan_atomic_int_get (struct kan_atomic_int_t *atomic)
{
    return SDL_AtomicGet ((SDL_AtomicInt *) atomic);
}
