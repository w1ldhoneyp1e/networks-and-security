#include "input.h"

#include "../common/parse.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int ClientReadNumber(int minimum, int maximum)
{
    char input[128];
    int number;

    for (;;) {
        printf("Enter an integer from %d to %d: ", minimum, maximum);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) {
            fprintf(stderr, "[client] input ended unexpectedly\n");
            return -1;
        }
        size_t length = strlen(input);

        while (length > 0 && isspace((unsigned char)input[length - 1])) {
            input[--length] = '\0';
        }
        if (ParseIntRange(input, minimum, maximum, &number) == 0) {
            return number;
        }
        fprintf(stderr, "[client] please enter an integer from %d to %d\n", minimum, maximum);
    }
}
