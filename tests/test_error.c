/*
 * test_error.c — host test for the error mapping.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "error.h"
#include <stdio.h>

int main(void)
{
    const char* msg = robohub_error_to_string(ROBOHUB_OK);
    if (msg == NULL || msg[0] == '\0') {
        printf("FAIL: error message is empty\n");
        return 1;
    }
    printf("PASS: %s\n", msg);
    return 0;
}
