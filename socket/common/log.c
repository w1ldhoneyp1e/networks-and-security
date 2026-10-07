#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

void LogErrno(const char *component, const char *action)
{
    fprintf(stderr, "[%s] %s: %s\n", component, action, strerror(errno));
}
