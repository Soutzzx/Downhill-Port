/* CI-only harness: verify the native host without copyrighted game data. */
#include "native_host.h"
#include <windows.h>
#include <stdio.h>

int main(void) {
    if (!native_host_start()) {
        fprintf(stderr, "native host error: %s\n", native_host_error());
        return 1;
    }
    puts("NATIVE_HOST_READY");
    fflush(stdout);
    /* The integration tests connect within 20 seconds. */
    Sleep(20000);
    native_host_stop();
    return 0;
}
