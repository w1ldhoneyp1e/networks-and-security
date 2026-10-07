#ifndef SOCKET_SERVER_CONTROL_H
#define SOCKET_SERVER_CONTROL_H

#include <signal.h>

struct ServerControl {
    volatile sig_atomic_t stopRequested;
    int wakeupFd;
};

void ServerControlInit(struct ServerControl *control, int wakeupFd);
void ServerControlRequestStop(struct ServerControl *control);

#endif
