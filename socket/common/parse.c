#include "parse.h"

#include <errno.h>
#include <stdlib.h>

int ParseIntRange(const char *text, int minimum, int maximum, int *value)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < minimum || parsed > maximum) {
        return -1;
    }
    *value = (int)parsed;
    return 0;
}
