#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_timer.h>

#include <kan/threading/atomic.h>

struct kan_atomic_int_t kan_atomic_int_init (int value)
{
    struct kan_atomic_int_t atomic;
    atomic.value = value;
    return atomic;
}

static_assert (sizeof (struct kan_atomic_int_t) == sizeof (SDL_SpinLock),
               "Check that spin lock and Kan atomic are the same.");

void kan_atomic_int_lock (struct kan_atomic_int_t *atomic) { SDL_LockSpinlock ((SDL_SpinLock *) atomic); }

bool kan_atomic_int_try_lock (struct kan_atomic_int_t *atomic)
{
    // It is SDL_bool, so it correctly converts to our types.
    return SDL_TryLockSpinlock ((SDL_SpinLock *) atomic);
}

void kan_atomic_int_unlock (struct kan_atomic_int_t *atomic) { SDL_UnlockSpinlock ((SDL_SpinLock *) atomic); }

static_assert (sizeof (struct kan_atomic_int_t) == sizeof (SDL_AtomicInt),
               "Check that SDL atomic and Kan atomic are the same.");

#define RW_LOCK_DO_LOCK(IS_LOCKED, VALUE_CHANGE)                                                                       \
    kan_loop_size_t iterations = 0u;                                                                                   \
    while (true)                                                                                                       \
    {                                                                                                                  \
        const int old_value = SDL_GetAtomicInt ((SDL_AtomicInt *) atomic);                                             \
        if (!(old_value IS_LOCKED))                                                                                    \
        {                                                                                                              \
            const int new_value = old_value VALUE_CHANGE;                                                              \
            if (SDL_CompareAndSwapAtomicInt ((SDL_AtomicInt *) atomic, old_value, new_value))                          \
            {                                                                                                          \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        /* Just mirror SDL3 waiting style. */                                                                          \
        if (iterations < 32u)                                                                                          \
        {                                                                                                              \
            ++iterations;                                                                                              \
            SDL_CPUPauseInstruction ();                                                                                \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            iterations = 0u;                                                                                           \
            SDL_DelayNS (0u);                                                                                          \
        }                                                                                                              \
    }

void kan_atomic_int_lock_read (struct kan_atomic_int_t *atomic) { RW_LOCK_DO_LOCK (< 0, +1) }

void kan_atomic_int_unlock_read (struct kan_atomic_int_t *atomic) { SDL_AddAtomicInt ((SDL_AtomicInt *) atomic, -1); }

void kan_atomic_int_lock_write (struct kan_atomic_int_t *atomic) { RW_LOCK_DO_LOCK (!= 0, -1) }

void kan_atomic_int_unlock_write (struct kan_atomic_int_t *atomic) { SDL_AddAtomicInt ((SDL_AtomicInt *) atomic, 1); }

#undef RW_LOCK_DO_LOCK

int kan_atomic_int_add (struct kan_atomic_int_t *atomic, int delta)
{
    return SDL_AddAtomicInt ((SDL_AtomicInt *) atomic, delta);
}

int kan_atomic_int_set (struct kan_atomic_int_t *atomic, int new_value)
{
    return SDL_SetAtomicInt ((SDL_AtomicInt *) atomic, new_value);
}

bool kan_atomic_int_compare_and_set (struct kan_atomic_int_t *atomic, int old_value, int new_value)
{
    return SDL_CompareAndSwapAtomicInt ((SDL_AtomicInt *) atomic, old_value, new_value);
}

int kan_atomic_int_get (struct kan_atomic_int_t *atomic) { return SDL_GetAtomicInt ((SDL_AtomicInt *) atomic); }
