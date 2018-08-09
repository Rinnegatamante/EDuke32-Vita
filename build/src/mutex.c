#include "compat.h"
#include "mutex.h"

int32_t mutex_init(mutex_t *mutex)
{
#ifdef RENDERTYPEWIN
    *mutex = CreateMutex(0, FALSE, 0);
    return (*mutex == 0);
#elif defined _3DS
    return svcCreateMutex(mutex, false);
#elif defined __PSP2__
	*mutex = sceKernelCreateMutex("Mutex", 0, 0, NULL);
	return (*mutex > 0);
#else
    if (mutex)
    {
        *mutex = SDL_CreateMutex();
        if (*mutex != NULL)
            return 0;
    }
    return -1;
#endif
}

int32_t mutex_lock(mutex_t *mutex)
{
#ifdef RENDERTYPEWIN
    return (WaitForSingleObject(*mutex, INFINITE) == WAIT_FAILED);
#elif defined _3DS
    return svcWaitSynchronization(*mutex, U64_MAX);
#elif defined __PSP2__
	return sceKernelLockMutex(*mutex, 1, NULL);
#else
    return SDL_LockMutex(*mutex);
#endif
}

int32_t mutex_unlock(mutex_t *mutex)
{
#ifdef RENDERTYPEWIN
    return (ReleaseMutex(*mutex) == 0);
#elif defined _3DS
    return svcReleaseMutex(*mutex);
#elif defined __PSP2__
	return sceKernelUnlockMutex(*mutex, 1);
#else
    return SDL_UnlockMutex(*mutex);
#endif
}
