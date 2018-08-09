#ifndef build_interface_layer_
#define build_interface_layer_ CTR

#include "compat.h"
#include "baselayer.h"

static inline void idle(void)
{
    sceKernelDelayThread(10);
}

#endif // build_interface_layer_