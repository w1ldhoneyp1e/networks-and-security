#include "control.h"

#include <stdint.h>
#include <unistd.h>

void ServerControlInit(struct ServerControl *control, int wakeupFd)
{
    control->stopRequested = 0;
    control->wakeupFd = wakeupFd;
}

void ServerControlRequestStop(struct ServerControl *control)
{
    const uint64_t one = 1;

    control->stopRequested = 1;
    if (control->wakeupFd >= 0) {
        ssize_t result;

        result = write(control->wakeupFd, &one, sizeof(one));
        (void)result;
    }
}
